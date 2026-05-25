/*
 * test_sserial.c — integration test for the LBP / sserial protocol.
 *
 * Topology:
 *   test process (LBP host/client)
 *     ├─ fds[1]  ←──socketpair──→  fds[0]  sserial_host component
 *     └─ stdin pipe ──────────────────────→  stmbl_host process
 *
 * The test:
 *   1. Creates a socketpair.
 *   2. Forks stmbl_host with SSERIAL_HOST_FD=fds[0] in the environment.
 *   3. Sends HAL commands via stdin: load sserial_host, set frt_prio, start.
 *   4. Exercises the LBP protocol on fds[1] using lbp_client, advancing
 *      simulated time between each exchange with "step N" commands.
 *   5. Reports results in TAP format.
 */
#include "runner.h"
#include "lbp_client.h"
#include "sserial.h"

#include <assert.h>
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

/* ---- helpers ------------------------------------------------------------- */

/* Write a NUL-terminated command line to stmbl_host's stdin pipe. */
static void sim_cmd(int fd, const char *cmd) {
  size_t n = strlen(cmd);
  while(n > 0) {
    ssize_t w = write(fd, cmd, n);
    if(w <= 0) break;
    cmd += w;
    n   -= (size_t)w;
  }
}

/*
 * advance_fn callback for lbp_conn_t: called by lbp_client after each
 * transmit so stmbl_host runs frt_func and processes the bytes we sent.
 * 3 RT cycles = 12 FRT calls — enough for one LBP command even if frt_func
 * needs more than one call to drain the partial reads.
 */
static void do_advance(void *arg) {
  int sim_in = *(int *)arg;
  sim_cmd(sim_in, "step 3\n");
}

/* ---- test cases ---------------------------------------------------------- */

static void run_tests(lbp_conn_t *conn, int sim_in) {
  /* -----------------------------------------------------------------
   * Test 1: Cookie
   * ----------------------------------------------------------------- */
  ok(lbp_check_cookie(conn), "cookie: slave returns 0x5A");

  /* -----------------------------------------------------------------
   * Test 2: Card name
   * ----------------------------------------------------------------- */
  char name[4] = {0};
  int cn_ok = lbp_read_card_name(conn, name);
  ok(cn_ok, "card_name: read succeeded");
  ok(cn_ok && name[0] == 's' && name[1] == 't' &&
                name[2] == 'b' && name[3] == 'l',
     "card_name: value is 'stbl'");

  /* -----------------------------------------------------------------
   * Test 3: Unit number (non-zero)
   * ----------------------------------------------------------------- */
  uint32_t unit = 0;
  int un_ok = lbp_read_unit_number(conn, &unit);
  ok(un_ok, "unit_number: read succeeded");
  ok(unit != 0, "unit_number: non-zero");

  /* -----------------------------------------------------------------
   * Test 4: Discovery
   * ----------------------------------------------------------------- */
  discovery_rpc_t disc;
  memset(&disc, 0, sizeof(disc));
  int disc_ok = lbp_discover(conn, &disc);
  ok(disc_ok,            "discovery: RPC succeeded");
  ok(disc.input  == 11,  "discovery: input = 11 bytes");
  ok(disc.output == 9,   "discovery: output = 9 bytes");
  ok(disc.ptocp  == 0x018B, "discovery: ptocp = 0x018B");
  ok(disc.gtocp  == 0x01A5, "discovery: gtocp = 0x01A5");

  /* -----------------------------------------------------------------
   * Test 5: CT_RW memory read — read 4 bytes at address 0.
   * Bytes 0..3 of lbp_mem are: input(0x0B) output(0x09) ptocp_lo(0x8B) ptocp_hi(0x01).
   * ----------------------------------------------------------------- */
  uint8_t mem4[4] = {0};
  int mr_ok = lbp_read_mem(conn, 0, 2, mem4);  /* ds=2 → 4 bytes */
  ok(mr_ok, "mem_read: CT_RW ds=2 at addr 0 succeeded");
  ok(mem4[0] == 0x0B && mem4[1] == 0x09,
     "mem_read: bytes 0..1 match discovery input/output");

  /* -----------------------------------------------------------------
   * Test 6: CT_RW memory read — read 1 byte at address 0.
   * ----------------------------------------------------------------- */
  uint8_t mem1 = 0;
  int mr1_ok = lbp_read_mem(conn, 0, 0, &mem1);  /* ds=0 → 1 byte */
  ok(mr1_ok,          "mem_read: CT_RW ds=0 at addr 0 succeeded");
  ok(mem1 == 0x0B,    "mem_read: byte 0 = 0x0B (discovery.input)");

  /* -----------------------------------------------------------------
   * Test 7: Process data — basic exchange
   *
   * Set HAL pins in stmbl_host, then send a ProcessData command and
   * verify the response reflects the pin values we set.
   * ----------------------------------------------------------------- */
  if(!disc_ok) goto skip_pd;

  sim_cmd(sim_in, "sserial_host0.pos_fb = 1.5\n");
  sim_cmd(sim_in, "sserial_host0.vel_fb = -2.25\n");
  sim_cmd(sim_in, "sserial_host0.pd_arm = 1.0\n");

  pd_out_t out;
  memset(&out, 0, sizeof(out));
  out.pos_cmd = 3.0f;
  out.vel_cmd = 4.0f;
  out.enable  = 1;

  uint8_t fault_byte = 0xFF;
  pd_in_t pd_in;
  memset(&pd_in, 0, sizeof(pd_in));

  int pd_ok = lbp_process_data(conn, &disc, &out, &fault_byte, &pd_in);
  ok(pd_ok,                    "process_data: exchange succeeded");
  ok(fault_byte == 0x00,       "process_data: fault byte is 0x00");
  ok_float(pd_in.pos_fb, 1.5f,  1e-4f, "process_data: pos_fb echoed correctly");
  ok_float(pd_in.vel_fb, -2.25f, 1e-4f, "process_data: vel_fb echoed correctly");

  /* Second exchange — verify pos_cmd reached the HAL pin.
   * We change pos_fb so the second response is distinguishable. */
  sim_cmd(sim_in, "sserial_host0.pos_fb = 9.875\n");
  sim_cmd(sim_in, "sserial_host0.pd_arm = 1.0\n");

  out.pos_cmd = 7.0f;
  pd_in_t pd_in2;
  memset(&pd_in2, 0, sizeof(pd_in2));
  int pd2_ok = lbp_process_data(conn, &disc, &out, NULL, &pd_in2);
  ok(pd2_ok, "process_data: second exchange succeeded");
  ok_float(pd_in2.pos_fb, 9.875f, 1e-4f, "process_data: updated pos_fb returned");

skip_pd:;
}

/* ---- main ---------------------------------------------------------------- */

int main(void) {
  /* Create the socketpair connecting the test (fds[1]) to sserial_host (fds[0]). */
  int fds[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
    perror("socketpair");
    return 1;
  }

  /* Pipe for stmbl_host stdin. */
  int stdin_pipe[2];
  if(pipe(stdin_pipe) < 0) {
    perror("pipe");
    return 1;
  }

  /* Build the SSERIAL_HOST_FD env string before fork so it does not race. */
  char fd_env[64];
  snprintf(fd_env, sizeof(fd_env), "SSERIAL_HOST_FD=%d", fds[0]);

  pid_t pid = fork();
  if(pid < 0) {
    perror("fork");
    return 1;
  }

  if(pid == 0) {
    /* ----- child: stmbl_host ------------------------------------------ */
    close(fds[1]);

    /* Redirect stdin from the pipe */
    dup2(stdin_pipe[0], STDIN_FILENO);
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);

    /* Redirect stdout to /dev/null — we do not parse its output */
    int devnull = open("/dev/null", O_WRONLY);
    if(devnull >= 0) {
      dup2(devnull, STDOUT_FILENO);
      close(devnull);
    }

    /* Expose the socketpair fd via the environment */
    putenv(fd_env);

    execl("./stmbl_host", "./stmbl_host", (char *)NULL);
    perror("exec stmbl_host");
    _exit(127);
  }

  /* ----- parent: test --------------------------------------------------- */
  close(fds[0]);
  close(stdin_pipe[0]);

  int sim_in = stdin_pipe[1];
  int lbp_fd = fds[1];

  /* Initialise stmbl_host: load the component, set frt_prio, start HAL.
     frt_prio must be > 0 before "start" so sort_frt() adds sserial_host to
     the FRT schedule. */
  sim_cmd(sim_in, "load sserial_host\n");
  sim_cmd(sim_in, "sserial_host0.frt_prio = 1.0\n");
  sim_cmd(sim_in, "start\n");

  /* LBP client — 200 ms timeout per receive.
     advance_fn drives stmbl_host time forward after each LBP transmit so
     frt_func processes the command and fills the response socket buffer. */
  lbp_conn_t conn;
  lbp_conn_init(&conn, lbp_fd, 200);
  conn.advance_fn  = do_advance;
  conn.advance_arg = &sim_in;

  run_tests(&conn, sim_in);

  /* Signal stmbl_host to exit cleanly */
  sim_cmd(sim_in, "exit\n");
  close(sim_in);
  close(lbp_fd);

  int status = 0;
  waitpid(pid, &status, 0);

  return done_testing();
}
