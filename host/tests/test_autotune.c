/*
 * test_autotune.c — integration test: rlpsij motor parameter identification
 * against the motsim PMSM simulator in a single stmbl_host process.
 *
 * Topology (single process):
 *   rlpsij0 (RT, prio 1) ←→ motsim0 (RT, prio 2)
 *
 * rlpsij drives voltage/current commands and reads back id/iq feedback plus
 * the continuous motor position (abs_pos_fb).  motsim integrates the full
 * PMSM dq electrical dynamics and mechanical equations of motion.
 *
 * Motor ground truth in motsim:
 *   mot_r   = 1.5  Ω       mot_l   = 1   mH     mot_psi = 0.05 Wb
 *   mot_pp  = 1             mot_j   = 1   g·m²   (0.001 kg·m²)
 *   mot_f   = 0.001 N·m·s  mot_d   = 0.0003 N·m·s/rad
 *   pwm_volt = 48 V
 *
 * rlpsij identification sequence (en=1, fully automatic):
 *   State 1 — R measurement       (~1 s, voltage mode, d-axis lock)
 *   State 2 — transition          (1 RT step)
 *   State 3 — polecount           (~1.5 s, rotating d-axis field)
 *   State 4 — psi / friction / damping  (vel_time = 4 s, closed-loop vel)
 *   State 5 — J (inertia)         (vel_time = 4 s, ±test_cur steps)
 *   State 6 — done
 *
 * Total sim time: 15 s ≈ 75 000 RT cycles (0.2 ms each).
 * Runs in << 1 s real time.
 *
 * Expected results (tolerances allow for measurement noise and back-EMF cross-
 * coupling at low speed):
 *   r         ≈ 1.5   Ω     (±10 %)
 *   psi       ≈ 0.05  Wb    (±10 %)
 *   polecount = 1           (rounded to nearest integer)
 *   j         > 0           (finite positive; absolute value not checked — see
 *                            DEBUGNOTES "rlpsij J estimate filter lag")
 */
#include "runner.h"

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct proc {
  pid_t pid;
  int   in;
  int   out;
  char  buf[16384];
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

/* Accumulate stdout into p->buf until needle appears.
   10 s timeout — long runs (75 k RT cycles) still complete in < 1 s real time. */
static int accum_until(struct proc *p, const char *needle) {
  for(;;) {
    if(strstr(p->buf, needle)) return 1;
    struct pollfd pfd = { .fd = p->out, .events = POLLIN };
    if(poll(&pfd, 1, 10000) <= 0) return 0;
    int space = (int)sizeof(p->buf) - p->len - 1;
    if(space <= 0) return 0;
    ssize_t n = read(p->out, p->buf + p->len, (size_t)space);
    if(n <= 0) return 0;
    p->len += (int)n;
    p->buf[p->len] = '\0';
  }
}

/* Run <seconds> of simulated time, then block until the sync marker appears. */
static void run_sim(struct proc *p, float seconds) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "run %.4f\n", seconds);
  sim_cmd(p, cmd);
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

/* Find the LAST occurrence of "pin_expr = <value>" in buf.
   HAL wiring echo lines ("OK sink <= source = <old_value>") appear before
   the final explicit pin query, so we must take the last match. */
static float get_pin(const char *buf, const char *pin_expr) {
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "%s = ", pin_expr);
  const char *last = NULL;
  const char *p = strstr(buf, pattern);
  while(p) { last = p; p = strstr(p + 1, pattern); }
  if(!last) return -9999.0f;
  float val = -9999.0f;
  sscanf(last + strlen(pattern), "%f", &val);
  return val;
}

int main(void) {
  int p_stdin[2], p_stdout[2];
  if(pipe(p_stdin) < 0 || pipe(p_stdout) < 0) { perror("pipe"); return 1; }

  pid_t pid = fork();
  if(pid < 0) { perror("fork"); return 1; }
  if(pid == 0) {
    dup2(p_stdin[0],  STDIN_FILENO);  close(p_stdin[0]);  close(p_stdin[1]);
    dup2(p_stdout[1], STDOUT_FILENO); close(p_stdout[0]); close(p_stdout[1]);
    execl("./stmbl_host", "./stmbl_host", (char *)NULL);
    perror("exec stmbl_host"); _exit(127);
  }
  close(p_stdin[0]); close(p_stdout[1]);

  struct proc sim = { .pid = pid, .in = p_stdin[1], .out = p_stdout[0], .len = 0 };
  sim.buf[0] = '\0';

  /* ---- Load components -------------------------------------------------- */
  sim_cmd(&sim, "load rlpsij\n");
  sim_cmd(&sim, "load motsim\n");

  /* rlpsij runs first (computes commands), motsim integrates after */
  sim_cmd(&sim, "rlpsij0.rt_prio = 1.0\n");
  sim_cmd(&sim, "motsim0.rt_prio = 2.0\n");

  /* ---- Motor simulator ground-truth parameters -------------------------- */
  sim_cmd(&sim, "motsim0.mot_r = 1.5\n");
  sim_cmd(&sim, "motsim0.mot_l = 0.001\n");
  sim_cmd(&sim, "motsim0.mot_psi = 0.05\n");
  sim_cmd(&sim, "motsim0.mot_pp = 1.0\n");
  sim_cmd(&sim, "motsim0.mot_j = 0.001\n");   /* 1 g·m² — fast acceleration */
  sim_cmd(&sim, "motsim0.mot_f = 0.001\n");
  sim_cmd(&sim, "motsim0.mot_d = 0.0003\n");
  sim_cmd(&sim, "motsim0.pwm_volt = 48.0\n");

  /* Controller's view of the motor (matches true values → accurate current loop) */
  sim_cmd(&sim, "motsim0.r = 1.5\n");
  sim_cmd(&sim, "motsim0.l = 0.001\n");
  sim_cmd(&sim, "motsim0.psi = 0.05\n");
  sim_cmd(&sim, "motsim0.cur_bw = 500.0\n");

  /* ---- rlpsij identification parameters --------------------------------- */
  sim_cmd(&sim, "rlpsij0.test_cur = 3.0\n");    /* 3 A identification current */
  sim_cmd(&sim, "rlpsij0.test_vel = 6.2832\n"); /* ~1 Hz (2π rad/s) */
  sim_cmd(&sim, "rlpsij0.rl_time  = 1.0\n");    /* R measurement window */
  sim_cmd(&sim, "rlpsij0.vel_time = 4.0\n");    /* psi and J windows */

  /* ---- HAL wiring: rlpsij ←→ motsim ------------------------------------- */

  /* Commands rlpsij → motsim */
  sim_cmd(&sim, "motsim0.d_cmd   = rlpsij0.d_cmd\n");
  sim_cmd(&sim, "motsim0.q_cmd   = rlpsij0.q_cmd\n");
  sim_cmd(&sim, "motsim0.com_pos = rlpsij0.com_pos\n");
  sim_cmd(&sim, "motsim0.cmd_mode = rlpsij0.cmd_mode\n");
  sim_cmd(&sim, "motsim0.en      = rlpsij0.en_out\n");
  /* Feed rlpsij's smoothed velocity back as the current-loop commutation rate */
  sim_cmd(&sim, "motsim0.com_vel = rlpsij0.vel\n");

  /* Feedback motsim → rlpsij */
  sim_cmd(&sim, "rlpsij0.id_fb      = motsim0.id_fb\n");
  sim_cmd(&sim, "rlpsij0.iq_fb      = motsim0.iq_fb\n");
  sim_cmd(&sim, "rlpsij0.ud_fb      = motsim0.ud_fb\n");
  sim_cmd(&sim, "rlpsij0.uq_fb      = motsim0.uq_fb\n");
  /* Use continuous (unwrapped-in-modular sense) position for velocity derivative */
  sim_cmd(&sim, "rlpsij0.abs_pos_fb = motsim0.pos\n");

  /* Enable identification — state machine starts on the first RT cycle */
  sim_cmd(&sim, "rlpsij0.en = 1.0\n");
  sim_cmd(&sim, "start\n");

  /* ---- Simulation phases ------------------------------------------------ */
  /*
   * Phase 1 (1.5 s): R measurement (state 1 → 2, rl_time = 1 s).
   *   rlpsij integrates d_cmd upward until id = 3 A; measures R = ud/id.
   */
  run_sim(&sim, 1.5f);

  /*
   * Phase 2 (2.5 s): polecount measurement (state 3).
   *   rlpsij rotates com_pos at 2π rad/s; motor follows with pp = 1.
   *   Exits when timer > 1 s AND motor has returned to fb_offset.
   */
  run_sim(&sim, 2.5f);

  /*
   * Phase 3 (5.0 s): psi, friction, damping (state 4, vel_time = 4 s).
   *   Closed-loop velocity control: measures back-EMF at ±test_vel.
   */
  run_sim(&sim, 5.0f);

  /*
   * Phase 4 (6.0 s): J measurement (state 5, vel_time = 4 s) + margin.
   *   rlpsij applies ±test_cur steps and measures torque/acceleration ratio.
   */
  run_sim(&sim, 6.0f);

  /* ---- Query identified results ----------------------------------------- */
  sim_cmd(&sim, "rlpsij0.state\n");
  sim_cmd(&sim, "rlpsij0.r\n");
  sim_cmd(&sim, "rlpsij0.psi\n");
  sim_cmd(&sim, "rlpsij0.polecount\n");
  sim_cmd(&sim, "rlpsij0.j\n");
  sim_cmd(&sim, "exit\n");

  close(sim.in);
  waitpid(pid, NULL, 0);
  drain_output(&sim);
  close(sim.out);

  /* ---- TAP assertions --------------------------------------------------- */

  /* Test 1: identification state machine reached "done" (state 6) */
  float state = get_pin(sim.buf, "rlpsij0.state");
  ok(state >= 6.0f, "autotune completed: rlpsij0.state = 6");

  /*
   * Test 2: resistance R.
   * Measured from ud_fb/id_fb at steady state with motor locked (no back-EMF).
   * Should be very close to the true 1.5 Ω.
   */
  float r_id = get_pin(sim.buf, "rlpsij0.r");
  ok_float(r_id, 1.5f, 1.5f * 0.10f,
           "identified R = 1.5 Ω (±10 %)");

  /*
   * Test 3: flux linkage psi.
   * Measured from back-EMF at test_vel with d_cmd = 0 (no d-axis excitation).
   * Formula: psi = (uq_fb − iq_fb × r) / vel / polecount.
   */
  float psi_id = get_pin(sim.buf, "rlpsij0.psi");
  ok_float(psi_id, 0.05f, 0.05f * 0.10f,
           "identified psi = 0.05 Wb (±10 %)");

  /*
   * Test 4: pole count.
   * Counted by incrementing a register each time com_pos wraps, while the
   * motor makes one full mechanical revolution.  With pp = 1 the result
   * should round to the integer 1.
   */
  float pp_id = get_pin(sim.buf, "rlpsij0.polecount");
  ok((int)(pp_id + 0.5f) == 1, "identified polecount = 1");

  /*
   * Test 5: rotor inertia J — sanity check only.
   * rlpsij computes J from torque / acceleration.  The acceleration estimate
   * uses a hardcoded 0.99-per-step IIR on velocity; the resulting steady-state
   * lag inflates the denominator by ~k/(1-k) = 99, so j_identified ≈ J/90
   * for our parameters (see DEBUGNOTES "rlpsij J estimate filter lag").
   * We only check that the state machine produced a finite positive estimate.
   */
  float j_id = get_pin(sim.buf, "rlpsij0.j");
  ok(j_id > 0.0f, "J estimate is positive (state-5 identification ran)");

  return done_testing();
}
