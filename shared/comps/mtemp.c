#include "mtemp_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "mtemp.h"

// The motor's temperature sensor on the HV board (hv0.mot_temp, ohms) and
// the thermal model (iit0.temp): temp, what fault0 checks, is the higher of
// the two. type (shared/mtemp.h): 0 none (the model alone, as before),
// 1 KTY84-130, 2 PT1000, 3 PTC, 4 NTC, 5 NC thermal switch. A tripped PTC
// or switch, and an open or shorted sensor wire, read as trip + 10 degC, so
// fault0 stops the drive (MOT_TEMP_ERROR); err says which.
HAL_COMP(mtemp);

HAL_PIN(type);
HAL_PIN(r);         // in: the sensor's resistance [ohm] (hv0.mot_temp)
HAL_PIN(model);     // in: the thermal model's temperature [degC] (iit0.temp)
HAL_PIN(trip);      // in: the trip temperature [degC] (conf0.max_mot_temp)
HAL_PIN(ptc_r);     // PTC: tripped above this [ohm]
HAL_PIN(ntc_r25);   // NTC: resistance at 25 degC [ohm]
HAL_PIN(ntc_beta);  // NTC: B value [K]
HAL_PIN(tau);       // the sensor reading's filter [s]

HAL_PIN(sensor);    // out: the sensor's temperature [degC] (PTC, switch: 0 cold, trip + 10 tripped)
HAL_PIN(temp);      // out: max(sensor, model) [degC]
HAL_PIN(tripped);   // out: PTC / switch open
HAL_PIN(err);       // out: 0 ok, 1 open wire, 2 short (filtered over 0.1 s)

struct mtemp_ctx_t {
  float r;     // filtered resistance
  float bad;   // how long the wire has looked broken [s]
  int init;
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct mtemp_ctx_t *ctx      = (struct mtemp_ctx_t *)ctx_ptr;
  struct mtemp_pin_ctx_t *pins = (struct mtemp_pin_ctx_t *)pin_ptr;
  PIN(trip)     = 100.0;
  PIN(ptc_r)    = 1330.0;
  PIN(ntc_r25)  = 10000.0;
  PIN(ntc_beta) = 3950.0;
  PIN(tau)      = 0.2;
  ctx->init     = 0;
  ctx->bad      = 0.0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct mtemp_ctx_t *ctx      = (struct mtemp_ctx_t *)ctx_ptr;
  struct mtemp_pin_ctx_t *pins = (struct mtemp_pin_ctx_t *)pin_ptr;
  int type                     = (int)PIN(type);
  float model                  = PIN(model);
  if(type <= MTEMP_NONE || type >= MTEMP_TYPES) {
    PIN(sensor)  = 0.0;
    PIN(temp)    = model;
    PIN(tripped) = 0.0;
    PIN(err)     = 0.0;
    return;
  }
  float r = PIN(r);
  int wire = mtemp_wire(type, r, PIN(ntc_r25));
  // a broken wire must last 0.1 s (a spike doesn't count); the filter
  // only sees good readings
  ctx->bad = wire ? ctx->bad + period : 0.0;
  if(!wire) {
    if(!ctx->init) {
      ctx->r    = r;
      ctx->init = 1;
    }
    ctx->r += (r - ctx->r) * MIN(period / MAX(PIN(tau), period), 1.0);
  }
  PIN(err)  = ctx->bad > 0.1 ? wire : 0.0;
  float hot = PIN(trip) + 10.0;
  float s;
  switch(type) {
    case MTEMP_PTC:
      PIN(tripped) = ctx->init && ctx->r > PIN(ptc_r);
      s            = PIN(tripped) > 0.0 ? hot : 0.0;
      break;
    case MTEMP_SWITCH:
      PIN(tripped) = r > 100.0 || r < 0.0 || !(r == r);
      s            = PIN(tripped) > 0.0 ? hot : 0.0;
      break;
    default:
      PIN(tripped) = 0.0;
      s            = ctx->init ? mtemp_t_of(type, ctx->r, PIN(ntc_r25), PIN(ntc_beta)) : model;
  }
  if(PIN(err) > 0.0) {
    s = hot;
  }
  PIN(sensor) = s;
  PIN(temp)   = MAX(s, model);
}

hal_comp_t mtemp_comp_struct = {
    .name      = "mtemp",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct mtemp_ctx_t),
    .pin_count = sizeof(struct mtemp_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
