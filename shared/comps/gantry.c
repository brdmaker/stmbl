#include "gantry_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

// gantry: one of two drives moving a gantry's bridge (Y1, Y2), each with
// its own scale. Each drive is tuned for its share of the moving mass, but
// in the bridge's yaw (Y1 against Y2) it moves only 2 J / L^2 of it, often
// a quarter or less: its loops are that much faster there and can fight
// the other side at a few hundred Hz (DEBUGNOTES 2026-10-21).
//
// With the partner's position error and velocity (a drive-to-drive link),
// each drive feeds its pid a blend: the common mode (the mean of both) as
// it is, the yaw (the difference) times r:
//   v' = v + kv (v_partner - v),  kv = (1 - r_vel) / 2
//   e' = e + kp (e_partner - e),  kp = (1 - r_pos) / 2
// so the yaw's velocity loop gain is r_vel times the common mode's:
// r_vel = m_yaw / m_side tunes it for the yaw's own inertia (the fight is
// there, in the fast loop). r_pos scales the yaw's position loop the same
// way; at 1 (default) the bridge is held square as stiffly as before.
// r_vel = r_pos = 1, or no link: the drive alone.
// Errors, not positions, are exchanged: the two scales' offsets don't
// matter, only that both drives get the same command (a gantry's Y joints).
//
//   pid0.pos_fb = gantry0.pos_out, pid0.vel_fb = gantry0.vel_out
//   gantry0.cmd = (pid0's pos_ext_cmd source), pos_in, vel_in: the feedback
//   err_tx, vel_tx: to the partner; err_rx, vel_rx, rx_ok: from it
HAL_COMP(gantry);

HAL_PIN(en);
HAL_PIN(r_vel);  // the yaw's velocity loop gain relative to the common mode's (1: off)
HAL_PIN(r_pos);  // the same for the position loop
HAL_PIN(lim);  // largest correction of the error [rad] (a bad partner)

HAL_PIN(cmd);
HAL_PIN(pos_in);
HAL_PIN(vel_in);
HAL_PIN(pos_out);
HAL_PIN(vel_out);

HAL_PIN(err_tx);  // own position error [rad] and velocity [rad/s], to the partner
HAL_PIN(vel_tx);
HAL_PIN(err_rx);  // the partner's
HAL_PIN(vel_rx);
HAL_PIN(rx_ok);   // the link is up (the transport sets it)

HAL_PIN(yaw);     // e_partner - e [rad]: the bridge's yaw error, as this side sees it
HAL_PIN(active);

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct gantry_pin_ctx_t *pins = (struct gantry_pin_ctx_t *)pin_ptr;
  PIN(r_vel) = 1.0;
  PIN(r_pos) = 1.0;
  PIN(lim) = 0.5;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct gantry_pin_ctx_t *pins = (struct gantry_pin_ctx_t *)pin_ptr;

  float e = minus(PIN(cmd), PIN(pos_in));
  PIN(err_tx) = e;
  PIN(vel_tx) = PIN(vel_in);

  float kv = 0.5f * (1.0f - CLAMP(PIN(r_vel), 0.0f, 1.0f));
  float kp = 0.5f * (1.0f - CLAMP(PIN(r_pos), 0.0f, 1.0f));
  int on   = PIN(en) > 0.0f && PIN(rx_ok) > 0.0f && (kv > 0.0f || kp > 0.0f);
  float de = PIN(err_rx) - e;
  PIN(yaw) = de;
  if(on) {
    float c       = CLAMP(kp * de, -PIN(lim), PIN(lim));
    PIN(pos_out)  = mod(PIN(pos_in) - c);  // e' = e + c
    PIN(vel_out)  = PIN(vel_in) + kv * (PIN(vel_rx) - PIN(vel_in));
    PIN(active)   = 1.0;
  } else {
    PIN(pos_out) = PIN(pos_in);
    PIN(vel_out) = PIN(vel_in);
    PIN(active)  = 0.0;
  }
}

hal_comp_t gantry_comp_struct = {
    .name      = "gantry",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = 0,
    .pin_count = sizeof(struct gantry_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
