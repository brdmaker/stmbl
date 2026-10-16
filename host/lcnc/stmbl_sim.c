/*
 * stmbl_sim: a simulated STMBL drive as a LinuxCNC realtime component.
 *
 * Runs the drive's control code (host/ build of shared/ and the drive config
 * templates) against a simulated motor, inside the servo thread, in lockstep
 * with LinuxCNC. Its pins mirror the hm2 smart-serial pins of a real drive
 * (hm2_<board>.0.stbl.0.0.*), so a HAL file switches between the two by
 * renaming pins. con_cmd / con_fb carry the drive's console
 * (linuxcnc/sserialterm).
 *
 *   loadrt stmbl_sim conf=/path/stmbl.conf [sim=/path/sim.txt] [tty=/tmp/stmbl-sim]
 *   addf stmbl-sim.0 servo-thread
 *
 * conf is the drive's flash config: the drive boots from it, and
 * flashsaveconf over the console writes it back. sim holds sim-only HAL
 * commands, parsed after host/sim_board.txt. tty is where the drive's USB
 * console appears (a pty), for servoterm-cli -d /tmp/stmbl-sim.
 *
 * Only one drive per LinuxCNC instance: the drive's HAL is a single global.
 * uspace only (the console uses a pty and a thread).
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include "stmbl_sim_api.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("simulated STMBL servo drive");
MODULE_LICENSE("GPL");

static char *conf = "";
RTAPI_MP_STRING(conf, "drive config (flash) file");
static char *sim = "";
RTAPI_MP_STRING(sim, "sim-only HAL commands, parsed after sim_board");
static char *tty = "/tmp/stmbl-sim";
RTAPI_MP_STRING(tty, "path of the drive console pty symlink, empty for none");

#define PREFIX "stmbl-sim.0"

typedef struct {
  hal_float_t *pos_cmd;
  hal_float_t *vel_cmd;
  hal_float_t *pos_fb;
  hal_float_t *vel_fb;
  hal_float_t *current;
  hal_float_t *power;
  hal_bit_t *enable;
  hal_bit_t *fault;
  hal_bit_t *index_enable;
  hal_u32_t *con_cmd, *con_fb;  // the console (shared/sscon.h), as hm2's con_cmd / con_fb
  hal_bit_t *out[4];
  hal_bit_t *in[4];
  hal_float_t *scale;
} pins_t;

static int comp_id;
static pins_t *pins;
static stmbl_sim_pd_t pd;

static void update(void *arg, long period) {
  pins_t *p = arg;

  pd.pos_cmd      = *p->pos_cmd;
  pd.vel_cmd      = *p->vel_cmd;
  pd.scale        = *p->scale;
  pd.enable       = *p->enable;
  pd.index_enable = *p->index_enable;
  pd.con_cmd      = (uint16_t)(*p->con_cmd & 0x3ff);
  for(int i = 0; i < 4; i++) {
    pd.out[i] = *p->out[i];
  }

  stmbl_sim_update(&pd, period);

  *p->pos_fb  = pd.pos_fb;
  *p->vel_fb  = pd.vel_fb;
  *p->current = pd.current;
  *p->power   = pd.power;
  *p->con_fb  = pd.con_fb;
  *p->fault   = pd.fault;
  for(int i = 0; i < 4; i++) {
    *p->in[i] = pd.in[i];
  }
  // The feedback answers this exchange. If it asked for the index and the
  // drive no longer echoes it, the index was found: clear the pin, as hm2
  // does.
  if(pd.index_enable && !pd.index_enable_fb) {
    *p->index_enable = 0;
  }
}

static int export_pins(void) {
  int r = 0;
  r |= hal_pin_float_newf(HAL_IN, &pins->pos_cmd, comp_id, PREFIX ".pos_cmd");
  r |= hal_pin_float_newf(HAL_IN, &pins->vel_cmd, comp_id, PREFIX ".vel_cmd");
  r |= hal_pin_float_newf(HAL_OUT, &pins->pos_fb, comp_id, PREFIX ".pos_fb");
  r |= hal_pin_float_newf(HAL_OUT, &pins->vel_fb, comp_id, PREFIX ".vel_fb");
  r |= hal_pin_float_newf(HAL_OUT, &pins->current, comp_id, PREFIX ".current");
  r |= hal_pin_float_newf(HAL_OUT, &pins->power, comp_id, PREFIX ".power");
  r |= hal_pin_u32_newf(HAL_IN, &pins->con_cmd, comp_id, PREFIX ".con_cmd");
  r |= hal_pin_u32_newf(HAL_OUT, &pins->con_fb, comp_id, PREFIX ".con_fb");
  r |= hal_pin_float_newf(HAL_IN, &pins->scale, comp_id, PREFIX ".scale");
  r |= hal_pin_bit_newf(HAL_IN, &pins->enable, comp_id, PREFIX ".enable");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->fault, comp_id, PREFIX ".fault");
  r |= hal_pin_bit_newf(HAL_IO, &pins->index_enable, comp_id, PREFIX ".index_enable");
  for(int i = 0; i < 4 && !r; i++) {
    r |= hal_pin_bit_newf(HAL_IN, &pins->out[i], comp_id, PREFIX ".out-%02d", i);
    r |= hal_pin_bit_newf(HAL_OUT, &pins->in[i], comp_id, PREFIX ".in-%02d", i);
  }
  return r;
}

int rtapi_app_main(void) {
  comp_id = hal_init("stmbl_sim");
  if(comp_id < 0) {
    return comp_id;
  }
  if(!conf || !conf[0]) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_sim: conf=<drive config file> is required\n");
    hal_exit(comp_id);
    return -EINVAL;
  }

  pins = hal_malloc(sizeof(pins_t));
  if(!pins || export_pins()) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_sim: cannot export pins\n");
    hal_exit(comp_id);
    return -ENOMEM;
  }

  int r = stmbl_sim_open(conf, sim, tty);
  if(r) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_sim: cannot start the drive (%s %s): %d\n",
                    conf, r == -2 ? sim : (r == -3 ? tty : ""), r);
    hal_exit(comp_id);
    return -EINVAL;
  }

  if(hal_export_funct(PREFIX, update, pins, 1, 0, comp_id)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmbl_sim: cannot export function\n");
    stmbl_sim_close();
    hal_exit(comp_id);
    return -EINVAL;
  }

  rtapi_print_msg(RTAPI_MSG_INFO, "stmbl_sim: drive from %s, console on %s\n", conf, tty);
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  stmbl_sim_close();
  hal_exit(comp_id);
}
