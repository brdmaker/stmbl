/*
 * test_sserial_motor.c — integration test: LBP velocity command drives a
 * virtual veltopos motor and position feedback is read back via sserial.
 *
 * Topology:
 *   test process (LBP host)
 *     ├─ fds[1]  ←──socketpair──→  fds[0]  sserial_host  (FRT)
 *     └─ stdin pipe ──────────────────────→  stmbl_host
 *
 * HAL wiring inside stmbl_host:
 *   veltopos0.vel  ← sserial_host0.vel_cmd   (velocity command from LBP master)
 *   sserial_host0.pos_fb ← veltopos0.pos     (position fed back over LBP)
 *
 * With polecount=1, period=0.2ms, vel_cmd=10 rad/s the integrator advances
 * 0.002 rad per RT cycle.  Each "step 3" (3 RT cycles) adds 0.006 rad, so
 * position read-back values are predictable and exactly testable.
 */
#include "runner.h"
#include "lbp_client.h"
#include "sserial.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Process-data structs — must match sserial_host.c */
#pragma pack(push, 1)
typedef struct {
  float    pos_cmd;
  float    vel_cmd;
  uint32_t out_0        : 1;
  uint32_t out_1        : 1;
  uint32_t out_2        : 1;
  uint32_t out_3        : 1;
  uint32_t enable       : 1;
  uint32_t index_enable : 1;
  uint32_t padding      : 2;
} pd_out_t;  /* 9 bytes */

typedef struct {
  float    pos_fb;
  float    vel_fb;
  int8_t   current;
  uint32_t in_0         : 1;
  uint32_t in_1         : 1;
  uint32_t in_2         : 1;
  uint32_t in_3         : 1;
  uint32_t fault        : 1;
  uint32_t index_enable : 1;
  uint32_t padding      : 2;
} pd_in_t;   /* 10 bytes */
#pragma pack(pop)

static void sim_cmd(int fd, const char *cmd) {
  size_t n = strlen(cmd);
  while(n > 0) {
    ssize_t w = write(fd, cmd, n);
    if(w <= 0) break;
    cmd += w;
    n   -= (size_t)w;
  }
}

static void do_advance(void *arg) {
  int sim_in = *(int *)arg;
  sim_cmd(sim_in, "step 3\n");
}

/*
 * Arm one ProcessData exchange and run it.
 * pd_arm must be set via the pipe before the LBP bytes enter the socket so
 * stmbl_host's fgets loop commits pd_arm=1 before run_cycles starts.
 */
static int pd_exchange(lbp_conn_t *conn, const discovery_rpc_t *disc,
                       int sim_in, pd_out_t *out, pd_in_t *in) {
  memset(in, 0, sizeof(*in));
  sim_cmd(sim_in, "sserial_host0.pd_arm = 1.0\n");
  return lbp_process_data(conn, disc, out, NULL, in);
}

static void run_tests(lbp_conn_t *conn, int sim_in) {
  /* ------------------------------------------------------------------ *
   * Test 1: Discovery                                                   *
   * ------------------------------------------------------------------ */
  discovery_rpc_t disc;
  memset(&disc, 0, sizeof(disc));
  int disc_ok = lbp_discover(conn, &disc);
  ok(disc_ok, "motor: discovery succeeded");
  if(!disc_ok) return;

  /*
   * Exchanges 1–2: vel_cmd = 10 rad/s
   *
   * RT period = 0.2 ms, polecount = 1, so per RT cycle:
   *   Δpos = vel_cmd × polecount × period = 10 × 1 × 0.0002 = 0.002 rad
   *
   * sserial_host reads pos_fb at the start of FRT call 1 of each step,
   * BEFORE that step's RT cycles run.  Therefore each exchange reports
   * the position ACCUMULATED by ALL PREVIOUS steps:
   *   exchange 1 pos_fb = pos after 0 cycles = 0.0
   *   exchange 2 pos_fb = pos after 3 cycles = 0.006
   *   exchange 3 pos_fb = pos after 6 cycles = 0.012
   *   exchange 4 (vel=0) pos_fb = pos after 9 cycles = 0.018
   *   exchange 5 (vel=0) pos_fb = 0.018 (unchanged)
   */
  pd_out_t out;
  memset(&out, 0, sizeof(out));
  out.vel_cmd = 10.0f;
  out.enable  = 1;

  /* Exchange 1 */
  pd_in_t pd1;
  int ok1 = pd_exchange(conn, &disc, sim_in, &out, &pd1);
  ok(ok1, "motor: exchange 1 succeeded");
  ok_float(pd1.pos_fb, 0.0f, 1e-4f, "motor: exchange 1 pos_fb = 0.0 (initial)");

  /* Exchange 2 */
  pd_in_t pd2;
  int ok2 = pd_exchange(conn, &disc, sim_in, &out, &pd2);
  ok(ok2, "motor: exchange 2 succeeded");
  ok(pd2.pos_fb > pd1.pos_fb, "motor: exchange 2 pos_fb increased (vel_cmd=10)");
  ok_float(pd2.pos_fb, 0.006f, 1e-4f, "motor: exchange 2 pos_fb = 0.006 (3 RT cycles)");

  /* Exchange 3 — still vel=10; reads pos after exchange 2's 3 RT cycles.
     After this exchange stmbl_host runs 3 more RT cycles at vel=10:
     pos goes from 0.012 → 0.018. */
  pd_in_t pd3;
  int ok3 = pd_exchange(conn, &disc, sim_in, &out, &pd3);
  ok(ok3, "motor: exchange 3 succeeded");
  ok_float(pd3.pos_fb, 0.012f, 1e-4f, "motor: exchange 3 pos_fb = 0.012 (6 RT cycles)");

  /* ------------------------------------------------------------------ *
   * Exchanges 4–5: vel_cmd = 0 — integrator should hold position       *
   *                                                                     *
   * Exchange 4 is the first to send vel_cmd=0.  Its step 3 sets        *
   * vel_cmd=0 during the first FRT call, then runs 3 RT cycles at      *
   * vel=0.  pos_fb read by exchange 4 = pos AFTER exchange 3's step 3  *
   * (which ran 3 more cycles at vel=10) = 0.018.                       *
   * Exchange 5 then confirms the position is still 0.018 (no movement).*
   * ------------------------------------------------------------------ */
  out.vel_cmd = 0.0f;

  pd_in_t pd4;
  int ok4 = pd_exchange(conn, &disc, sim_in, &out, &pd4);
  ok(ok4, "motor: exchange 4 (vel=0) succeeded");
  ok_float(pd4.pos_fb, 0.018f, 1e-4f, "motor: exchange 4 pos_fb = 0.018 (9 RT cycles)");

  pd_in_t pd5;
  int ok5 = pd_exchange(conn, &disc, sim_in, &out, &pd5);
  ok(ok5, "motor: exchange 5 (vel=0) succeeded");
  /* vel=0 took effect in exchange 4's step 3 — pos must not have changed */
  ok_float(pd5.pos_fb, pd4.pos_fb, 1e-4f, "motor: pos_fb held when vel=0");
}

int main(void) {
  int fds[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
    perror("socketpair");
    return 1;
  }

  int stdin_pipe[2];
  if(pipe(stdin_pipe) < 0) {
    perror("pipe");
    return 1;
  }

  char fd_env[64];
  snprintf(fd_env, sizeof(fd_env), "SSERIAL_HOST_FD=%d", fds[0]);

  pid_t pid = fork();
  if(pid < 0) {
    perror("fork");
    return 1;
  }

  if(pid == 0) {
    close(fds[1]);
    dup2(stdin_pipe[0], STDIN_FILENO);
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);

    int devnull = open("/dev/null", O_WRONLY);
    if(devnull >= 0) {
      dup2(devnull, STDOUT_FILENO);
      close(devnull);
    }

    putenv(fd_env);
    execl("./stmbl_host", "./stmbl_host", (char *)NULL);
    perror("exec stmbl_host");
    _exit(127);
  }

  close(fds[0]);
  close(stdin_pipe[0]);

  int sim_in = stdin_pipe[1];
  int lbp_fd = fds[1];

  /*
   * Wire the virtual motor:
   *   sserial_host  (FRT slave) — LBP interface
   *   veltopos      (RT)        — velocity integrator: vel → pos
   *
   * veltopos0.vel    ← sserial_host0.vel_cmd   (command from LBP master)
   * sserial_host0.pos_fb ← veltopos0.pos       (position echoed back)
   *
   * max_acc=1e6 disables ramp limiting so vel_cmd takes effect immediately.
   */
  sim_cmd(sim_in, "load sserial_host\n");
  sim_cmd(sim_in, "load veltopos\n");
  sim_cmd(sim_in, "veltopos0.polecount = 1.0\n");
  sim_cmd(sim_in, "veltopos0.max_vel = 1000.0\n");
  sim_cmd(sim_in, "veltopos0.max_acc = 1000000.0\n");
  sim_cmd(sim_in, "veltopos0.vel = sserial_host0.vel_cmd\n");
  sim_cmd(sim_in, "sserial_host0.pos_fb = veltopos0.pos\n");
  sim_cmd(sim_in, "sserial_host0.frt_prio = 1.0\n");
  sim_cmd(sim_in, "veltopos0.rt_prio = 1.0\n");
  sim_cmd(sim_in, "start\n");

  lbp_conn_t conn;
  lbp_conn_init(&conn, lbp_fd, 200);
  conn.advance_fn  = do_advance;
  conn.advance_arg = &sim_in;

  run_tests(&conn, sim_in);

  sim_cmd(sim_in, "exit\n");
  close(sim_in);
  close(lbp_fd);

  int status = 0;
  waitpid(pid, &status, 0);

  return done_testing();
}
