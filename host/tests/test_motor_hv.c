/*
 * test_motor_hv.c — integration test: PMSM motor simulation on the HV (F3) side.
 *
 * Topology:
 *   F4 process (stmbl_host)          F3 process (stmbl_host)
 *     hv_lv_host0  (RT)  ←─socketpair─→  ls_host0   (RT, prio 1)
 *       en=1, cmd_mode=0                   |  ↑ receives d/q voltage cmds
 *       q_cmd=5 V  ──────────────────────▶ |
 *       id_fb, iq_fb ◀────────────────────   |  ↓ pins wired in HAL
 *                                          motsim0  (RT, prio 2)
 *                                            PMSM dq dynamics
 *                                            id_fb, iq_fb → ls_host0
 *
 * F4 applies 5 V on the q-axis (voltage mode, en=1).  The motor is at
 * standstill with large inertia (J=0.1 kg·m²), so speed barely changes
 * over the test window.  Expected steady-state q-axis current:
 *
 *   iq_ss = vq / R = 5.0 / 1.5 = 3.333 A   (no back-EMF at ≈0 speed)
 *
 * The electrical time constant τ = L/R = 1 ms; the test runs ≈10 ms per
 * phase (50 RT steps × 0.2 ms), so 10τ of settling time — current is
 * fully at steady state before F4 queries the pins.
 *
 * Test sequence uses stdout-marker synchronisation (see test_hv_link.c).
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

struct proc {
  pid_t pid;
  int   in;
  int   out;
  char  buf[8192];
  int   len;
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

static int accum_until(struct proc *p, const char *needle) {
  for(;;) {
    if(strstr(p->buf, needle)) return 1;
    struct pollfd pfd = { .fd = p->out, .events = POLLIN };
    if(poll(&pfd, 1, 5000) <= 0) return 0;
    int space = (int)sizeof(p->buf) - p->len - 1;
    if(space <= 0) return 0;
    ssize_t n = read(p->out, p->buf + p->len, (size_t)space);
    if(n <= 0) return 0;
    p->len += (int)n;
    p->buf[p->len] = '\0';
  }
}

static void run_phase(struct proc *p, int n) {
  for(int i = 0; i < n; i++) sim_cmd(p, "step 1\n");
  sim_cmd(p, "time\n");
  accum_until(p, "sim time:");
}

static void drain_output(struct proc *p) {
  while(p->len < (int)sizeof(p->buf) - 1) {
    ssize_t n = read(p->out, p->buf + p->len,
                     (size_t)(sizeof(p->buf) - 1 - p->len));
    if(n <= 0) break;
    p->len += (int)n;
    p->buf[p->len] = '\0';
  }
}

static float get_pin(const char *buf, const char *pin_expr) {
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "%s = ", pin_expr);
  /* Use last occurrence: setup wiring lines print the same pattern with stale values */
  const char *last = NULL;
  const char *p = strstr(buf, pattern);
  while(p) { last = p; p = strstr(p + 1, pattern); }
  if(!last) return -9999.0f;
  float val = -9999.0f;
  sscanf(last + strlen(pattern), "%f", &val);
  return val;
}

int main(void) {
  int fds[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) { perror("socketpair"); return 1; }

  int f4_stdin[2], f3_stdin[2], f4_stdout[2], f3_stdout[2];
  if(pipe(f4_stdin) < 0 || pipe(f3_stdin) < 0 ||
     pipe(f4_stdout) < 0 || pipe(f3_stdout) < 0) { perror("pipe"); return 1; }

  char f4_env[64], f3_env[64];
  snprintf(f4_env, sizeof(f4_env), "HV_LV_HOST_FD=%d", fds[0]);
  snprintf(f3_env, sizeof(f3_env), "HV_F3_HOST_FD=%d", fds[1]);

  pid_t f4_pid = fork();
  if(f4_pid < 0) { perror("fork F4"); return 1; }
  if(f4_pid == 0) {
    close(fds[1]);
    dup2(f4_stdin[0],  STDIN_FILENO);  close(f4_stdin[0]);  close(f4_stdin[1]);
    dup2(f4_stdout[1], STDOUT_FILENO); close(f4_stdout[0]); close(f4_stdout[1]);
    close(f3_stdin[0]); close(f3_stdin[1]);
    close(f3_stdout[0]); close(f3_stdout[1]);
    putenv(f4_env);
    execl("./stmbl_host", "./stmbl_host", (char *)NULL);
    perror("exec stmbl_host (F4)"); _exit(127);
  }

  pid_t f3_pid = fork();
  if(f3_pid < 0) { perror("fork F3"); return 1; }
  if(f3_pid == 0) {
    close(fds[0]);
    dup2(f3_stdin[0],  STDIN_FILENO);  close(f3_stdin[0]);  close(f3_stdin[1]);
    dup2(f3_stdout[1], STDOUT_FILENO); close(f3_stdout[0]); close(f3_stdout[1]);
    close(f4_stdin[0]); close(f4_stdin[1]);
    close(f4_stdout[0]); close(f4_stdout[1]);
    putenv(f3_env);
    execl("./stmbl_host", "./stmbl_host", (char *)NULL);
    perror("exec stmbl_host (F3)"); _exit(127);
  }

  close(fds[0]); close(fds[1]);
  close(f4_stdin[0]); close(f3_stdin[0]);
  close(f4_stdout[1]); close(f3_stdout[1]);

  struct proc f4 = { .pid = f4_pid, .in = f4_stdin[1], .out = f4_stdout[0], .len = 0 };
  struct proc f3 = { .pid = f3_pid, .in = f3_stdin[1], .out = f3_stdout[0], .len = 0 };
  f4.buf[0] = f3.buf[0] = '\0';

  /* ---- F4: hv_lv_host in voltage mode, q-axis 5 V ---------------------- */
  sim_cmd(&f4, "load hv_lv_host\n");
  sim_cmd(&f4, "hv_lv_host0.r = 1.5\n");
  sim_cmd(&f4, "hv_lv_host0.l = 0.001\n");
  sim_cmd(&f4, "hv_lv_host0.psi = 0.05\n");
  sim_cmd(&f4, "hv_lv_host0.max_cur = 10.0\n");
  sim_cmd(&f4, "hv_lv_host0.scale = 1.0\n");
  sim_cmd(&f4, "hv_lv_host0.rt_prio = 1.0\n");
  sim_cmd(&f4, "hv_lv_host0.en = 1.0\n");       /* enable */
  sim_cmd(&f4, "hv_lv_host0.cmd_mode = 0.0\n"); /* voltage mode */
  sim_cmd(&f4, "hv_lv_host0.q_cmd = 5.0\n");    /* 5 V q-axis */
  sim_cmd(&f4, "start\n");

  /* ---- F3: ls_host + motsim wired for PMSM simulation ------------------ */
  sim_cmd(&f3, "load ls_host\n");
  sim_cmd(&f3, "load motsim\n");

  /* Scheduling: ls_host receives F4 packet first, then motsim integrates */
  sim_cmd(&f3, "ls_host0.rt_prio = 1.0\n");
  sim_cmd(&f3, "motsim0.rt_prio = 2.0\n");

  /* Voltage/enable commands from F4 → motor sim */
  sim_cmd(&f3, "motsim0.d_cmd = ls_host0.d_cmd\n");
  sim_cmd(&f3, "motsim0.q_cmd = ls_host0.q_cmd\n");
  sim_cmd(&f3, "motsim0.en = ls_host0.en\n");
  sim_cmd(&f3, "motsim0.cmd_mode = ls_host0.cmd_mode\n");

  /*
   * Motor parameters received from F4 via WRITE_CONF rotation.
   * Set initial non-zero values before the first packet arrives to prevent
   * divide-by-zero in the integrator (F4's first packet will update them
   * to the same values anyway since WRITE_CONF rotation starts at addr 0=r).
   */
  sim_cmd(&f3, "ls_host0.r = 1.5\n");
  sim_cmd(&f3, "ls_host0.l = 0.001\n");
  sim_cmd(&f3, "ls_host0.psi = 0.05\n");
  sim_cmd(&f3, "motsim0.mot_r = ls_host0.r\n");
  sim_cmd(&f3, "motsim0.mot_l = ls_host0.l\n");
  sim_cmd(&f3, "motsim0.mot_psi = ls_host0.psi\n");

  /* Power supply: 48 V on the HV board */
  sim_cmd(&f3, "ls_host0.dc_volt = 48.0\n");
  sim_cmd(&f3, "motsim0.pwm_volt = ls_host0.dc_volt\n");

  /*
   * Pole pairs = 1 (simplest): com_pos self-wired to pos so the forward and
   * inverse Park transforms cancel → commanded voltage = actual motor voltage.
   * Large inertia keeps the motor near standstill over the test window so the
   * back-EMF stays negligible and iq_ss ≈ vq/R.
   */
  sim_cmd(&f3, "motsim0.mot_pp = 1.0\n");
  sim_cmd(&f3, "motsim0.mot_j = 0.1\n");   /* 0.1 kg·m² — flywheel */
  sim_cmd(&f3, "motsim0.com_pos = motsim0.pos\n");
  sim_cmd(&f3, "motsim0.com_vel = motsim0.vel\n");

  /* Current feedback: motor sim → ls_host → F4 */
  sim_cmd(&f3, "ls_host0.id_fb = motsim0.id_fb\n");
  sim_cmd(&f3, "ls_host0.iq_fb = motsim0.iq_fb\n");
  sim_cmd(&f3, "ls_host0.ud_fb = motsim0.ud_fb\n");
  sim_cmd(&f3, "ls_host0.uq_fb = motsim0.uq_fb\n");

  sim_cmd(&f3, "start\n");

  /*
   * Synchronised phases (same pattern as test_hv_link):
   *   Phase 1 — F4 sends 50 voltage-command packets into the socket.
   *   Phase 2 — F3 reads those 50 packets; motsim integrates 50 RT steps.
   *             By step 50, iq has settled to ≈vq/R (≥10 time constants).
   *   Phase 3 — F4 reads F3's 50 replies (gets settled iq_fb).
   *   Phase 4 — F3 reads F4's Phase-3 config packets (gets r/l/psi).
   */
  run_phase(&f4, 50);
  run_phase(&f3, 50);
  run_phase(&f4, 50);
  run_phase(&f3, 50);

  /* Query pins of interest */
  sim_cmd(&f4, "hv_lv_host0.id_fb\n");
  sim_cmd(&f4, "hv_lv_host0.iq_fb\n");
  sim_cmd(&f4, "hv_lv_host0.dc_volt\n");
  sim_cmd(&f4, "hv_lv_host0.fault\n");
  sim_cmd(&f4, "exit\n");

  sim_cmd(&f3, "ls_host0.r\n");
  sim_cmd(&f3, "motsim0.vel\n");
  sim_cmd(&f3, "ls_host0.fault\n");
  sim_cmd(&f3, "exit\n");

  close(f4.in); close(f3.in);
  waitpid(f4_pid, NULL, 0); waitpid(f3_pid, NULL, 0);
  drain_output(&f4); drain_output(&f3);
  close(f4.out); close(f3.out);

  /* ------------------------------------------------------------------ *
   * Assertions                                                          *
   * ------------------------------------------------------------------ */

  /*
   * Test 1: q-axis current at steady state.
   * iq_ss = vq / R = 5.0 / 1.5 = 3.333 A.  5% tolerance covers the small
   * back-EMF from the slight motor acceleration.
   */
  float f4_iq_fb = get_pin(f4.buf, "hv_lv_host0.iq_fb");
  ok_float(f4_iq_fb, 5.0f / 1.5f, 5.0f / 1.5f * 0.05f,
           "F4: iq_fb = vq/R = 3.33 A (PMSM stall current from F3 motor sim)");

  /*
   * Test 2: d-axis current stays near zero (no d-axis voltage applied).
   * Tolerance is 5% of the q-axis current magnitude.
   */
  float f4_id_fb = get_pin(f4.buf, "hv_lv_host0.id_fb");
  ok_float(f4_id_fb, 0.0f, 5.0f / 1.5f * 0.05f,
           "F4: id_fb ≈ 0 (no d-axis excitation)");

  /* Test 3: F4 received F3's dc_volt state (48 V power supply) */
  float f4_dc_volt = get_pin(f4.buf, "hv_lv_host0.dc_volt");
  ok_float(f4_dc_volt, 48.0f, 0.1f, "F4: dc_volt = 48 V (F3 HV supply)");

  /* Test 4: F4 link is fault-free */
  float f4_fault = get_pin(f4.buf, "hv_lv_host0.fault");
  ok(f4_fault == 0.0f, "F4: no link fault");

  /* Test 5: F3 received motor resistance from F4 config */
  float f3_r = get_pin(f3.buf, "ls_host0.r");
  ok_float(f3_r, 1.5f, 1e-4f, "F3: r = 1.5 Ω received from F4 WRITE_CONF");

  /* Test 6: motor is slowly spinning (torque from iq accelerates the rotor) */
  float f3_vel = get_pin(f3.buf, "motsim0.vel");
  ok(f3_vel > 0.0f, "F3: motor velocity > 0 (torque from iq accelerates rotor)");

  /* Test 7: F3 link is fault-free */
  float f3_fault = get_pin(f3.buf, "ls_host0.fault");
  ok(f3_fault == 0.0f, "F3: no link fault");

  return done_testing();
}
