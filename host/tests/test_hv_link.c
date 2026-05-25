/*
 * test_hv_link.c — integration test for the dual-CPU inter-process link.
 *
 * Topology:
 *   F4 process (stmbl_host)          F3 process (stmbl_host)
 *     hv_lv_host0  (RT)  ←─socketpair─→  ls_host0  (RT)
 *     ↑ stdin_f4 pipe                      ↑ stdin_f3 pipe
 *
 * Protocol: packet_to_hv_t (F4→F3, 32 bytes, CRC-32/MPEG-2) and
 *           packet_from_hv_t (F3→F4, 32 bytes) — same as real hardware.
 *
 * Synchronised phase execution:
 *   After each phase a "time" command is sent; the parent reads the child's
 *   stdout until the "sim time:" marker appears, which guarantees all prior
 *   "step" commands were processed before the next phase starts.
 *
 *   Phase 1: run F4 × 50 steps → fills socket with 50 packets for F3
 *   Phase 2: run F3 × 50 steps → F3 reads those, sends 50 replies back
 *   Phase 3: run F4 × 50 steps → F4 reads F3's 50 replies
 *   Phase 4: run F3 × 50 steps → F3 reads F4's Phase 3 config packets
 *
 * After Phase 3, F4 has received F3's id_fb / iq_fb / dc_volt.
 * After Phase 4, F3 has received F4's motor parameters (r, l).
 * Both stdout pipes are accumulated and parsed for pin-query responses.
 */
#include "runner.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Per-process state kept in the parent. */
struct proc {
  pid_t pid;
  int   in;           /* write end of stdin pipe (parent → child) */
  int   out;          /* read end of stdout pipe (child → parent) */
  char  buf[8192];    /* accumulated stdout so far */
  int   len;          /* bytes in buf */
};

static void sim_cmd(struct proc *p, const char *cmd) {
  size_t n = strlen(cmd);
  while(n > 0) {
    ssize_t w = write(p->in, cmd, n);
    if(w <= 0) break;
    cmd += w;
    n   -= (size_t)w;
  }
}

/* Read from p->out into p->buf until needle appears.
   Returns 1 on success, 0 on timeout (5 s) or EOF. */
static int accum_until(struct proc *p, const char *needle) {
  for(;;) {
    if(strstr(p->buf, needle)) return 1;

    struct pollfd pfd = { .fd = p->out, .events = POLLIN };
    int r = poll(&pfd, 1, 5000);   /* 5 s timeout */
    if(r <= 0) return 0;           /* timeout or error */

    int space = (int)sizeof(p->buf) - p->len - 1;
    if(space <= 0) return 0;
    ssize_t n = read(p->out, p->buf + p->len, (size_t)space);
    if(n <= 0) return 0;
    p->len += (int)n;
    p->buf[p->len] = '\0';
  }
}

/* Run n step-1 cycles then issue a "time" sync marker.
   Block until the marker appears in the process stdout. */
static void run_phase(struct proc *p, int n) {
  for(int i = 0; i < n; i++) sim_cmd(p, "step 1\n");
  sim_cmd(p, "time\n");
  accum_until(p, "sim time:");
}

/* Read remaining stdout until EOF (call after waitpid). */
static void drain_output(struct proc *p) {
  while(p->len < (int)sizeof(p->buf) - 1) {
    ssize_t n = read(p->out, p->buf + p->len, (size_t)(sizeof(p->buf) - 1 - p->len));
    if(n <= 0) break;
    p->len += (int)n;
    p->buf[p->len] = '\0';
  }
}

/* Parse a "comp.pin = VALUE\n" line out of buf.  Returns the float value,
   or -9999.0 if the pattern is not found. */
static float get_pin(const char *buf, const char *pin_expr) {
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "%s = ", pin_expr);
  const char *p = strstr(buf, pattern);
  if(!p) return -9999.0f;
  float val = -9999.0f;
  sscanf(p + strlen(pattern), "%f", &val);
  return val;
}

int main(void) {
  /* socketpair: F4=fds[0], F3=fds[1] */
  int fds[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
    perror("socketpair");
    return 1;
  }

  /* stdin / stdout pipes for each process */
  int f4_stdin[2], f3_stdin[2];
  int f4_stdout[2], f3_stdout[2];

  if(pipe(f4_stdin) < 0 || pipe(f3_stdin) < 0 ||
     pipe(f4_stdout) < 0 || pipe(f3_stdout) < 0) {
    perror("pipe");
    return 1;
  }

  /* Env strings built before fork */
  char f4_env[64], f3_env[64];
  snprintf(f4_env, sizeof(f4_env), "HV_LV_HOST_FD=%d", fds[0]);
  snprintf(f3_env, sizeof(f3_env), "HV_F3_HOST_FD=%d", fds[1]);

  /* ---- Fork F4 process -------------------------------------------------- */
  pid_t f4_pid = fork();
  if(f4_pid < 0) { perror("fork F4"); return 1; }
  if(f4_pid == 0) {
    close(fds[1]);
    dup2(f4_stdin[0],  STDIN_FILENO);
    close(f4_stdin[0]); close(f4_stdin[1]);
    dup2(f4_stdout[1], STDOUT_FILENO);
    close(f4_stdout[0]); close(f4_stdout[1]);
    close(f3_stdin[0]); close(f3_stdin[1]);
    close(f3_stdout[0]); close(f3_stdout[1]);
    putenv(f4_env);
    execl("./stmbl_host", "./stmbl_host", (char *)NULL);
    perror("exec stmbl_host (F4)");
    _exit(127);
  }

  /* ---- Fork F3 process -------------------------------------------------- */
  pid_t f3_pid = fork();
  if(f3_pid < 0) { perror("fork F3"); return 1; }
  if(f3_pid == 0) {
    close(fds[0]);
    dup2(f3_stdin[0],  STDIN_FILENO);
    close(f3_stdin[0]); close(f3_stdin[1]);
    dup2(f3_stdout[1], STDOUT_FILENO);
    close(f3_stdout[0]); close(f3_stdout[1]);
    close(f4_stdin[0]); close(f4_stdin[1]);
    close(f4_stdout[0]); close(f4_stdout[1]);
    putenv(f3_env);
    execl("./stmbl_host", "./stmbl_host", (char *)NULL);
    perror("exec stmbl_host (F3)");
    _exit(127);
  }

  /* ---- Parent: close child-side fds ------------------------------------ */
  close(fds[0]); close(fds[1]);
  close(f4_stdin[0]); close(f3_stdin[0]);
  close(f4_stdout[1]); close(f3_stdout[1]);

  struct proc f4 = { .pid = f4_pid, .in = f4_stdin[1],
                     .out = f4_stdout[0], .len = 0 };
  struct proc f3 = { .pid = f3_pid, .in = f3_stdin[1],
                     .out = f3_stdout[0], .len = 0 };
  f4.buf[0] = f3.buf[0] = '\0';

  /* ---- Initialise F4 --------------------------------------------------- */
  sim_cmd(&f4, "load hv_lv_host\n");
  sim_cmd(&f4, "hv_lv_host0.r = 1.5\n");
  sim_cmd(&f4, "hv_lv_host0.l = 0.001\n");
  sim_cmd(&f4, "hv_lv_host0.psi = 0.05\n");
  sim_cmd(&f4, "hv_lv_host0.max_cur = 10.0\n");
  sim_cmd(&f4, "hv_lv_host0.scale = 1.0\n");
  sim_cmd(&f4, "hv_lv_host0.rt_prio = 1.0\n");
  sim_cmd(&f4, "start\n");

  /* ---- Initialise F3 --------------------------------------------------- */
  sim_cmd(&f3, "load ls_host\n");
  sim_cmd(&f3, "ls_host0.id_fb   = 2.5\n");
  sim_cmd(&f3, "ls_host0.iq_fb   = -1.25\n");
  sim_cmd(&f3, "ls_host0.dc_volt = 48.0\n");
  sim_cmd(&f3, "ls_host0.hv_temp = 35.0\n");
  sim_cmd(&f3, "ls_host0.rt_prio = 1.0\n");
  sim_cmd(&f3, "start\n");

  /*
   * Synchronised phases: each run_phase() blocks until all steps are
   * processed before the next phase begins.
   *
   * Phase 1 — F4 sends 50 packets into the socket buffer.
   * Phase 2 — F3 reads those 50 packets and sends 50 replies.
   * Phase 3 — F4 reads the 50 replies (gets id_fb / dc_volt / ...).
   * Phase 4 — F3 reads F4's Phase 3 config packets (gets r / l / ...).
   */
  run_phase(&f4, 50);   /* Phase 1: fills socket for F3 */
  run_phase(&f3, 50);   /* Phase 2: F3 reads & replies */
  run_phase(&f4, 50);   /* Phase 3: F4 reads replies */
  run_phase(&f3, 50);   /* Phase 4: F3 reads F4 config */

  /* Query F4 pins */
  sim_cmd(&f4, "hv_lv_host0.id_fb\n");
  sim_cmd(&f4, "hv_lv_host0.iq_fb\n");
  sim_cmd(&f4, "hv_lv_host0.dc_volt\n");
  sim_cmd(&f4, "hv_lv_host0.hv_temp\n");
  sim_cmd(&f4, "hv_lv_host0.fault\n");
  sim_cmd(&f4, "exit\n");

  /* Query F3 pins */
  sim_cmd(&f3, "ls_host0.r\n");
  sim_cmd(&f3, "ls_host0.l\n");
  sim_cmd(&f3, "ls_host0.psi\n");
  sim_cmd(&f3, "ls_host0.fault\n");
  sim_cmd(&f3, "exit\n");

  /* Close write ends so both processes get EOF after consuming "exit" */
  close(f4.in);
  close(f3.in);

  /* Wait for both processes to exit, then collect remaining stdout */
  waitpid(f4_pid, NULL, 0);
  waitpid(f3_pid, NULL, 0);
  drain_output(&f4);
  drain_output(&f3);

  close(f4.out);
  close(f3.out);

  /* ------------------------------------------------------------------ *
   * TAP assertions                                                      *
   * ------------------------------------------------------------------ */

  /* Test 1-2: F4 received current feedback from F3 */
  float f4_id_fb  = get_pin(f4.buf, "hv_lv_host0.id_fb");
  float f4_iq_fb  = get_pin(f4.buf, "hv_lv_host0.iq_fb");
  ok_float(f4_id_fb,  2.5f,   1e-4f, "F4: id_fb received from F3 (2.5 A)");
  ok_float(f4_iq_fb, -1.25f,  1e-4f, "F4: iq_fb received from F3 (-1.25 A)");

  /* Test 3: F4 received DC link voltage from F3 state rotation */
  float f4_dc_volt = get_pin(f4.buf, "hv_lv_host0.dc_volt");
  ok_float(f4_dc_volt, 48.0f, 1e-3f, "F4: dc_volt received from F3 state (48 V)");

  /* Test 4: F4 received HV board temperature from F3 state rotation */
  float f4_hv_temp = get_pin(f4.buf, "hv_lv_host0.hv_temp");
  ok_float(f4_hv_temp, 35.0f, 1e-3f, "F4: hv_temp received from F3 state (35 °C)");

  /* Test 5: F4 link is fault-free */
  float f4_fault = get_pin(f4.buf, "hv_lv_host0.fault");
  ok(f4_fault == 0.0f, "F4: no fault (link healthy after 100 exchanges)");

  /* Test 6-8: F3 received motor config from F4 WRITE_CONF rotation */
  float f3_r   = get_pin(f3.buf, "ls_host0.r");
  float f3_l   = get_pin(f3.buf, "ls_host0.l");
  float f3_psi = get_pin(f3.buf, "ls_host0.psi");
  ok_float(f3_r,   1.5f,   1e-4f, "F3: r received from F4 config (1.5 Ω)");
  ok_float(f3_l,   0.001f, 1e-6f, "F3: l received from F4 config (1 mH)");
  ok_float(f3_psi, 0.05f,  1e-5f, "F3: psi received from F4 config (0.05 Wb)");

  /* Test 9: F3 link is fault-free */
  float f3_fault = get_pin(f3.buf, "ls_host0.fault");
  ok(f3_fault == 0.0f, "F3: no fault (no timeout)");

  return done_testing();
}
