/*
 * powerquota: keeps a machine's fuses from blowing. It sums the stmbl
 * drives' input power (their smart-serial "power" pins) into quotas, one
 * per fuse, follows each fuse's heating against its trip profile and
 * slows the motion (motion.adaptive-feed) before one could blow. See
 * quota.h for the model and the config file, README.md for the wiring.
 *
 *   loadrt powerquota config=/path/quota.txt
 *   addf powerquota.0 servo-thread
 *
 * Pins:
 *   powerquota.0.power-N       in   drive N's input power [W] (N from 0)
 *   powerquota.0.feed-scale    out  -> motion.adaptive-feed (with M52 P1)
 *   powerquota.0.limiting      out  the feed is held below 1
 *   powerquota.0.total-power   out  the drives' sum [W], every period
 *   For meters and debugging:
 *   powerquota.0.total-power-avg  out  the sum low-passed over avg-time [W]
 *   powerquota.0.avg-time      in   its time constant [s] (default 1)
 *   powerquota.0.total-power-peak out  the highest sum since meter-reset [W]
 *   powerquota.0.energy        out  the drives' net energy since
 *                                   meter-reset [Wh] (regeneration counts
 *                                   negative)
 *   powerquota.0.meter-reset   in   rising edge: clears peak and energy
 *   powerquota.0.alarm         out  a fuse is about to blow even at the
 *                                   lowest feed (latched until reset)
 *   powerquota.0.alarm-quota   out  (s32) which (-1 none)
 *   powerquota.0.reset         in   rising edge: clears the alarm
 *   powerquota.0.chain-in      in   the estop chain ...
 *   powerquota.0.chain-out     out  ... and no alarm
 *   per quota NAME (the config's):
 *   powerquota.0.NAME.power    out  power through the fuse [W]
 *   powerquota.0.NAME.current  out  [A]
 *   powerquota.0.NAME.load     out  current / rated current
 *   powerquota.0.NAME.damage   out  heating, 1 = blown
 *   powerquota.0.NAME.scale    out  the feed scale it asks for
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include <string.h>

#include "quota.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("power quotas: slow the motion before a fuse blows");
MODULE_LICENSE("GPL");

static char *config = "";
RTAPI_MP_STRING(config, "quota config file (quota.h)");

#define PREFIX "powerquota.0"

typedef struct {
  hal_float_t *power[PQ_MAXD];
  hal_float_t *scale, *total, *avg, *avg_time, *peak, *energy;
  hal_bit_t *meter_reset;
  int last_meter_reset;
  hal_bit_t *limiting, *alarm, *reset, *chain_in, *chain_out;
  hal_s32_t *alarm_q;
  hal_float_t *qp[PQ_MAXQ], *qi[PQ_MAXQ], *qm[PQ_MAXQ], *qd[PQ_MAXQ], *qs[PQ_MAXQ];
  int last_reset;
} pins_t;

static int comp_id;
static pins_t *pins;
static pq_system sys;

static void update(void *arg, long period) {
  pins_t *p = arg;
  double pw[PQ_MAXD];
  for(int d = 0; d < sys.nd; d++) {
    pw[d] = *p->power[d];
  }
  if(*p->reset && !p->last_reset) {
    pq_reset(&sys);
  }
  p->last_reset = *p->reset;
  pq_step(&sys, pw, period * 1e-9);
  *p->scale = sys.scale;
  *p->total = sys.total;
  double dt = period * 1e-9, tc = *p->avg_time > 0.0 ? *p->avg_time : 1.0;
  if(*p->meter_reset && !p->last_meter_reset) {
    *p->peak = sys.total;
    *p->energy = 0.0;
  }
  p->last_meter_reset = *p->meter_reset;
  *p->avg += (sys.total - *p->avg) * dt / (tc + dt);
  *p->peak = sys.total > *p->peak ? sys.total : *p->peak;
  *p->energy += sys.total * dt / 3600.0;
  *p->limiting = sys.limiting;
  *p->alarm = sys.alarm;
  *p->alarm_q = sys.alarm_quota;
  *p->chain_out = *p->chain_in && !sys.alarm;
  for(int k = 0; k < sys.nq; k++) {
    *p->qp[k] = sys.q[k].p;
    *p->qi[k] = sys.q[k].i;
    *p->qm[k] = sys.q[k].m;
    *p->qd[k] = sys.q[k].damage;
    *p->qs[k] = sys.q[k].want;
  }
}

static int export_pins(void) {
  int r = 0;
  for(int d = 0; d < sys.nd && !r; d++) {
    r |= hal_pin_float_newf(HAL_IN, &pins->power[d], comp_id, PREFIX ".power-%d", d);
  }
  r |= hal_pin_float_newf(HAL_OUT, &pins->scale, comp_id, PREFIX ".feed-scale");
  r |= hal_pin_float_newf(HAL_OUT, &pins->total, comp_id, PREFIX ".total-power");
  r |= hal_pin_float_newf(HAL_OUT, &pins->avg, comp_id, PREFIX ".total-power-avg");
  r |= hal_pin_float_newf(HAL_IN, &pins->avg_time, comp_id, PREFIX ".avg-time");
  r |= hal_pin_float_newf(HAL_OUT, &pins->peak, comp_id, PREFIX ".total-power-peak");
  r |= hal_pin_float_newf(HAL_OUT, &pins->energy, comp_id, PREFIX ".energy");
  r |= hal_pin_bit_newf(HAL_IN, &pins->meter_reset, comp_id, PREFIX ".meter-reset");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->limiting, comp_id, PREFIX ".limiting");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->alarm, comp_id, PREFIX ".alarm");
  r |= hal_pin_s32_newf(HAL_OUT, &pins->alarm_q, comp_id, PREFIX ".alarm-quota");
  r |= hal_pin_bit_newf(HAL_IN, &pins->reset, comp_id, PREFIX ".reset");
  r |= hal_pin_bit_newf(HAL_IN, &pins->chain_in, comp_id, PREFIX ".chain-in");
  r |= hal_pin_bit_newf(HAL_OUT, &pins->chain_out, comp_id, PREFIX ".chain-out");
  for(int k = 0; k < sys.nq && !r; k++) {
    const char *n = sys.q[k].name;
    r |= hal_pin_float_newf(HAL_OUT, &pins->qp[k], comp_id, PREFIX ".%s.power", n);
    r |= hal_pin_float_newf(HAL_OUT, &pins->qi[k], comp_id, PREFIX ".%s.current", n);
    r |= hal_pin_float_newf(HAL_OUT, &pins->qm[k], comp_id, PREFIX ".%s.load", n);
    r |= hal_pin_float_newf(HAL_OUT, &pins->qd[k], comp_id, PREFIX ".%s.damage", n);
    r |= hal_pin_float_newf(HAL_OUT, &pins->qs[k], comp_id, PREFIX ".%s.scale", n);
  }
  if(!r) {
    *pins->scale = 1.0;
    *pins->avg_time = 1.0;
    *pins->alarm_q = -1;
  }
  return r;
}

int rtapi_app_main(void) {
  char err[256] = "";
  comp_id = hal_init("powerquota");
  if(comp_id < 0) {
    return comp_id;
  }
  pq_init(&sys);
  if(!config[0] || pq_file(&sys, config, err, sizeof(err))) {
    rtapi_print_msg(RTAPI_MSG_ERR, "powerquota: config=<file> needed: %s\n", err);
    hal_exit(comp_id);
    return -EINVAL;
  }
  pins = hal_malloc(sizeof(pins_t));
  if(!pins) {
    hal_exit(comp_id);
    return -ENOMEM;
  }
  memset(pins, 0, sizeof(*pins));
  if(export_pins() || hal_export_funct(PREFIX, update, pins, 1, 0, comp_id)) {
    rtapi_print_msg(RTAPI_MSG_ERR, "powerquota: cannot export pins / function\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  rtapi_print_msg(RTAPI_MSG_INFO, "powerquota: %d quotas, %d drives from %s\n", sys.nq, sys.nd, config);
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  hal_exit(comp_id);
}
