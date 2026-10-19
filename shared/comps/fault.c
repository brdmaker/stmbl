#include "fault_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"
#include "main.h"
#include "common.h"

HAL_COMP(fault);

HAL_PIN(en);
HAL_PIN(state);
HAL_PIN(fault);
HAL_PIN(last_fault);
HAL_PIN(en_out);
HAL_PIN(en_fb);
HAL_PIN(en_pid);

HAL_PIN(fb_ready);

HAL_PIN(cmd_error);
HAL_PIN(mot_fb_error);
HAL_PIN(com_fb_error);
HAL_PIN(joint_fb_error);
HAL_PIN(phase_error);  // fb_switch: autophasing failed
HAL_PIN(fb_mon_error);  // encoder monitor (encmon): feedback disagrees with the motor
HAL_PIN(hv_error);

HAL_PIN(hv_temp);
HAL_PIN(mot_temp);
HAL_PIN(max_hv_temp);
HAL_PIN(max_mot_temp);
HAL_PIN(high_hv_temp);
HAL_PIN(high_mot_temp);
HAL_PIN(fan_hv_temp);
HAL_PIN(fan_mot_temp);

HAL_PIN(scale);

HAL_PIN(dc_volt);
HAL_PIN(min_dc_volt);
HAL_PIN(high_dc_volt);
HAL_PIN(max_dc_volt);

HAL_PIN(dc_cur);
HAL_PIN(max_dc_cur);

HAL_PIN(ac_cur);
HAL_PIN(max_ac_cur);

HAL_PIN(pos_error);
HAL_PIN(max_pos_error);

HAL_PIN(sat);
HAL_PIN(max_sat);

HAL_PIN(mot_brake);
HAL_PIN(dc_brake);

HAL_PIN(hv_fan);
HAL_PIN(mot_fan);

HAL_PIN(print);

HAL_PIN(brake_release);

HAL_PIN(warn_timer);
HAL_PIN(error_timer);

// Stopping instead of coasting (docs/emergency_stop.md). Per cause, what
// happens when the drive leaves ENABLED: 0 outputs off (coast), 1 quick
// stop (qstop0: a controlled stop on the feedback), 2 dynamic brake
// (dbrake0: back-EMF into the windings, no feedback needed).
HAL_PIN(cmd_stop);   // CMD error (link lost)
HAL_PIN(fb_stop);    // a feedback error (1 is taken as 2)
HAL_PIN(err_stop);   // position, saturation, temperature, F4-measured DC bus errors
HAL_PIN(en_stop);    // en dropped while enabled
HAL_PIN(stop_time);  // [s] the longest stop; then outputs off
HAL_PIN(qstop);        // out: quick stop running
HAL_PIN(dbrake);       // out: dynamic brake running
HAL_PIN(qstop_done);   // in: qstop0.stopped
HAL_PIN(dbrake_done);  // in: dbrake0.stopped
HAL_PIN(stop);         // out: 0, 1 or 2
HAL_PIN(stop_timer);   // out: [s] into the stop

//fault strings for fault_t form common.h
static const char* fault_string[] = {
  "no error",
  "CMD error",
  "mot FB error",
  "com FB error",
  "joint FB error",
  "position error",
  "saturation error",
  "Motor overtemperture",
  "HV crc error",
  "HV timeout error",
  "HV overtemperture",
  "HV volt error",
  "HV fault error",
  "Current offset fault",
  "Motor overcurrent rms",
  "Motor overcurrent peak",
  "Motor overcurrent hw limit",
  "phasing failed (rotor blocked, or wrong motor/feedback setup)",
  "encoder monitor",
};

struct fault_ctx_t {
  state_t state;
  fault_t fault;
  float cmd_error;
  float mot_fb_error;
  float com_fb_error;
  float joint_fb_error;
  float hv_temp_error;
  float dc_volt_error;
  float mot_temp_error;
  int stop;     // 0 none, 1 quick stop, 2 dynamic brake
  int hv_fault; // this cycle's fault came from the HV board: outputs off
};

// the stop for a fault (fault.c's own checks; HV faults coast)
static int stop_for(struct fault_pin_ctx_t *pins, fault_t f, int hv) {
  if(hv) {
    return 0;
  }
  int m;
  switch(f) {
    case CMD_ERROR:
      m = PIN(cmd_stop);
      break;
    case MOT_FB_ERROR:
    case COM_FB_ERROR:
    case JOINT_FB_ERROR:
    case FB_MON_ERROR:  // the encoder monitor: the feedback can't be trusted
      m = PIN(fb_stop) > 0.0 ? 2 : 0;  // no feedback: no quick stop
      break;
    case POS_ERROR:
    case SAT_ERROR:
    case MOT_TEMP_ERROR:
    case HV_TEMP_ERROR:
    case HV_VOLT_ERROR:
      m = PIN(err_stop);
      break;
    default:
      m = 0;
  }
  return CLAMP(m, 0, 2);
}

void enable(char *ptr) {
  hal_parse("fault0.en = 1");
}

COMMAND("enable", enable, "enable");

void disable(char *ptr) {
  hal_parse("fault0.en = 0");
}

COMMAND("disable", disable, "disable");


static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fault_ctx_t *ctx      = (struct fault_ctx_t *)ctx_ptr;
  struct fault_pin_ctx_t *pins = (struct fault_pin_ctx_t *)pin_ptr;

  ctx->state         = DISABLED;
  ctx->fault         = NO_ERROR;
  PIN(last_fault)    = NO_ERROR;
  PIN(min_dc_volt)   = 20.0;
  PIN(high_dc_volt)  = 370.0;
  PIN(max_dc_volt)   = 390.0;
  PIN(max_hv_temp)   = 90.0;
  PIN(max_mot_temp)  = 100.0;
  PIN(high_hv_temp)  = 70.0;
  PIN(high_mot_temp) = 80.0;
  PIN(fan_hv_temp)   = 60.0;
  PIN(fan_mot_temp)  = 60.0;
  PIN(stop_time)     = 2.0;
  ctx->stop          = 0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fault_ctx_t *ctx      = (struct fault_ctx_t *)ctx_ptr;
  struct fault_pin_ctx_t *pins = (struct fault_pin_ctx_t *)pin_ptr;

  state_t before = ctx->state;
  ctx->hv_fault  = 0;

  switch(ctx->state) {
    case DISABLED:
      if(PIN(en) > 0.0 && ctx->stop == 0) {  // a stop runs to its end
        ctx->state = PHASING;
      }
      break;

    case ENABLED:
      if(PIN(en) <= 0.0) {
        ctx->state = DISABLED;
      }
      break;

    case PHASING:
      if(PIN(fb_ready) > 0.0) {
        ctx->state = ENABLED;
      }

      if(PIN(en) <= 0.0) {
        ctx->state = DISABLED;
      }
      break;

    case SOFT_FAULT:
      if(PIN(en) <= 0.0) {
        ctx->state = DISABLED;
      }
      break;

    case LED_TEST:
    case HARD_FAULT:
      break;
  }

  if(err_filter(&(ctx->cmd_error), 5.0, 0.001, PIN(cmd_error) > 0.0)) {
    ctx->fault      = CMD_ERROR;
    PIN(last_fault) = ctx->fault;
    ctx->state      = SOFT_FAULT;
  }

  if(err_filter(&(ctx->mot_fb_error), 5.0, 0.001, PIN(mot_fb_error) > 0.0)) {
    ctx->fault      = MOT_FB_ERROR;
    PIN(last_fault) = ctx->fault;
    ctx->state      = SOFT_FAULT;
  }

  if(err_filter(&(ctx->com_fb_error), 5.0, 0.001, PIN(com_fb_error) > 0.0)) {
    ctx->fault      = COM_FB_ERROR;
    PIN(last_fault) = ctx->fault;
    ctx->state      = SOFT_FAULT;
  }

  if(err_filter(&(ctx->joint_fb_error), 5.0, 0.001, PIN(joint_fb_error) > 0.0)) {
    ctx->fault      = JOINT_FB_ERROR;
    PIN(last_fault) = ctx->fault;
    ctx->state      = SOFT_FAULT;
  }

  if(PIN(phase_error) > 0.0 && ctx->state == PHASING) {
    ctx->fault      = PHASING_ERROR;
    PIN(last_fault) = ctx->fault;
    ctx->state      = SOFT_FAULT;
  }

  if(PIN(fb_mon_error) > 0.0) {  // encmon filters and times it already
    ctx->fault      = FB_MON_ERROR;
    PIN(last_fault) = ctx->fault;
    ctx->state      = SOFT_FAULT;
  }

  // a quick stop brakes at the limit on purpose: no position or
  // saturation error then
  if(ctx->stop != 1 && PIN(max_pos_error) > 0 && (ABS(PIN(pos_error)) > PIN(max_pos_error))) {
    ctx->fault      = POS_ERROR;
    PIN(last_fault) = ctx->fault;
    ctx->state      = SOFT_FAULT;
  }

  if(ctx->stop != 1 && PIN(max_sat) > 0 && (PIN(sat) > PIN(max_sat))) {
    ctx->fault      = SAT_ERROR;
    PIN(last_fault) = ctx->fault;
    ctx->state      = SOFT_FAULT;
  }

  if(err_filter(&(ctx->hv_temp_error), 5.0, 0.001, PIN(hv_temp) > PIN(max_hv_temp))) {
    ctx->fault      = HV_TEMP_ERROR;
    PIN(last_fault) = ctx->fault;
    ctx->state      = SOFT_FAULT;
  }

  if(err_filter(&(ctx->dc_volt_error), 5.0, 0.001, PIN(dc_volt) > PIN(max_dc_volt) || PIN(dc_volt) < PIN(min_dc_volt))) {
    ctx->fault      = HV_VOLT_ERROR;
    PIN(last_fault) = ctx->fault;
    ctx->state      = SOFT_FAULT;
  }

  if(err_filter(&(ctx->mot_temp_error), 5.0, 0.001, PIN(mot_temp) > PIN(max_mot_temp))) {
    ctx->fault      = MOT_TEMP_ERROR;
    PIN(last_fault) = ctx->fault;
    ctx->state      = SOFT_FAULT;
  }

  float hv_error = PIN(hv_error);
  if(hv_error > 0.0) {
    ctx->fault      = hv_error;
    PIN(last_fault) = ctx->fault;
    ctx->state      = SOFT_FAULT;
    ctx->hv_fault   = 1;
  }

  // leaving ENABLED while moving: stop as the cause says; a running stop
  // ends when the motor stands still, on an HV fault, or after stop_time,
  // and a feedback fault turns a quick stop into a dynamic brake
  if(ctx->stop == 0) {
    if(before == ENABLED && ctx->state == DISABLED) {
      // a link that times out drops enable with its error: a link loss
      ctx->stop = CLAMP((int)(PIN(cmd_error) > 0.0 ? PIN(cmd_stop) : PIN(en_stop)), 0, 2);
    } else if(before == ENABLED && ctx->state == SOFT_FAULT) {
      ctx->stop = stop_for(pins, ctx->fault, ctx->hv_fault);
    }
    PIN(stop_timer) = 0.0;
  } else {
    PIN(stop_timer) += period;
    int now = ctx->state == SOFT_FAULT ? stop_for(pins, ctx->fault, ctx->hv_fault) : ctx->stop;
    if(ctx->hv_fault) {
      ctx->stop = 0;
    } else if(ctx->stop == 1 && now == 2) {
      ctx->stop = 2;
    } else if(ctx->stop == 1 && ctx->state == SOFT_FAULT && now == 0 &&
              (ctx->fault == MOT_FB_ERROR || ctx->fault == COM_FB_ERROR || ctx->fault == JOINT_FB_ERROR ||
               ctx->fault == FB_MON_ERROR)) {
      ctx->stop = 0;  // no feedback and no dynamic brake: coast
    }
    if(PIN(stop_timer) > PIN(stop_time) || (ctx->stop == 1 && PIN(qstop_done) > 0.0) ||
       (ctx->stop == 2 && PIN(dbrake_done) > 0.0)) {
      ctx->stop = 0;
    }
  }

  float scale = 1.0;
  scale       = MIN(scale, SCALE(PIN(hv_temp), PIN(high_hv_temp), PIN(max_hv_temp)));
  scale       = MIN(scale, SCALE(PIN(dc_volt), PIN(high_dc_volt), PIN(max_dc_volt)));
  scale       = MIN(scale, SCALE(PIN(mot_temp), PIN(high_mot_temp), PIN(max_mot_temp)));
  scale       = MIN(scale, SCALE(PIN(ac_cur), PIN(max_ac_cur), PIN(max_ac_cur) * 1.1));
  scale       = MIN(scale, SCALE(PIN(dc_cur), PIN(max_dc_cur), PIN(max_dc_cur) * 1.1));

  PIN(dc_brake) = SCALE(PIN(dc_volt), PIN(max_dc_volt), PIN(high_dc_volt));

  if(PIN(hv_temp) >= PIN(fan_hv_temp)) {
    PIN(hv_fan) = 1.0;
  }

  if(PIN(hv_temp) < PIN(fan_hv_temp) * 0.9) {
    PIN(hv_fan) = 0.0;
  }

  if(PIN(mot_temp) >= PIN(fan_mot_temp)) {
    PIN(mot_fan) = 1.0;
  }

  if(PIN(mot_temp) < PIN(fan_mot_temp) * 0.9) {
    PIN(mot_fan) = 0.0;
  }

  switch(ctx->state) {
    case DISABLED:
      ctx->fault     = NO_ERROR;
      /* FALLTHRU */
    case SOFT_FAULT:
    case LED_TEST:
    case HARD_FAULT:
      PIN(mot_brake) = 0.0;
      PIN(en_out)    = 0.0;
      PIN(en_fb)     = 0.0;
      PIN(en_pid)    = 0.0;
      break;

    case ENABLED:
      PIN(mot_brake)  = 1.0;
      PIN(en_out)     = 1.0;
      PIN(en_fb)      = 1.0;
      PIN(en_pid)     = 1.0;
      ctx->fault      = NO_ERROR;
      PIN(last_fault) = NO_ERROR;
      break;

    case PHASING:
      PIN(mot_brake) = 1.0;
      ctx->fault     = NO_ERROR;
      PIN(en_pid)    = 0.0;
      PIN(en_fb)     = 1.0;
      PIN(en_out)    = 1.0;
      break;
  }

  // a stop keeps the power stage on: the quick stop with feedback and pid,
  // the dynamic brake without (dbrake0 holds the frame); the holding brake
  // stays released until the motor stands still
  PIN(qstop)  = ctx->stop == 1;
  PIN(dbrake) = ctx->stop == 2;
  PIN(stop)   = ctx->stop;
  if(ctx->stop) {
    PIN(mot_brake) = 1.0;
    PIN(en_out)    = 1.0;
    PIN(en_fb)     = ctx->stop == 1;
    PIN(en_pid)    = ctx->stop == 1;
  }

  PIN(fault) = ctx->fault;
  PIN(state) = ctx->state;
  PIN(scale) = scale;

  if(PIN(brake_release) > 0.0) {
    PIN(mot_brake) = 1.0;
  }

  PIN(warn_timer) = MAX(PIN(warn_timer) - period, 0.0);
  PIN(error_timer) = MAX(PIN(error_timer) - period, 0.0);
}


static void nrt_func(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct fault_ctx_t *ctx      = (struct fault_ctx_t *)ctx_ptr;
  struct fault_pin_ctx_t *pins = (struct fault_pin_ctx_t *)pin_ptr;

  if(PIN(warn_timer) <= 0.0){
    if(PIN(dc_volt) > PIN(high_dc_volt)){
      printf("<font color='orange'>WARNING:</font> over voltage current clamping active\n");
      PIN(warn_timer) = 1.0;
    }

    if(PIN(mot_temp) > PIN(high_mot_temp)){
      printf("<font color='orange'>WARNING:</font> over temperature (motor) current clamping active\n");
      PIN(warn_timer) = 1.0;
    }

    if(PIN(hv_temp) > PIN(high_hv_temp)){
      printf("<font color='orange'>WARNING:</font> over temperature (driver) current clamping active\n");
      PIN(warn_timer) = 1.0;
    }
  }

  //TODO: fix EDGE
  if(EDGE(ctx->state) || PIN(print) > 0.0) {
    PIN(print) = 0.0;
    switch((state_t)ctx->state) {
      case DISABLED:
        printf("INFO: Disabled \n");
        break;

      case ENABLED:
        printf("INFO: Enabled \n");
        break;

      case PHASING:
        printf("INFO: Phasing \n");
        break;

      case SOFT_FAULT:
        printf("ERROR: Fault %lu: %s\n",(uint32_t)ctx->fault,fault_string[(uint32_t)ctx->fault]);
        break;

      case HARD_FAULT:
        printf("ERROR: Hard fault: \n");
        break;

      default:
        break;
    }
  }
}

hal_comp_t fault_comp_struct = {
    .name      = "fault",
    .nrt       = nrt_func,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct fault_ctx_t),
    .pin_count = sizeof(struct fault_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
