#include "can_comp.h"
#include "hal.h"
#include "defines.h"
#include "canproto.h"
#include "can_hw.h"
#include "sscon.h"

// The drive's end of stmbl over CAN (shared/canproto.h, docs/can_support.md):
// in place of the smart-serial link, with the same pins, so the template
// (conf/template/can.txt) is sserial.txt with can0 for sserial0.
//
// LinuxCNC's side (linuxcnc/stmblcan) sends, every servo period, this
// node's command and then SYNC. At SYNC the newest command goes to the
// pins (as a smart-serial frame would), and the feedback is sent back:
// position, velocity (or, every 8th period, a slot value: current,
// power, temperature, supply), flags and the console field.
//
//   can0.node = 1          this drive's node ID (1..127)
//   can0.vel_scale = 100   full scale of vel_cmd / vel_fb on the wire
//   can0.scale = 1         linrev's scale (smart-serial gets it from
//                          LinuxCNC; here it is set in the config)
//   can0.connected         SYNC seen within the timeout
//   can0.syncs, frames     counters
// Discovery: asked on 0x600 + node, it hands out its description
// (canp_desc_text: device stbl, profile stmbl1, vel_scale) in 7-byte blocks.

HAL_COMP(can);

HAL_PIN(node);
HAL_PIN(vel_scale);
HAL_PIN(error);
HAL_PIN(connected);
HAL_PIN(timeout);  // [s]
HAL_PIN(pos_cmd);
HAL_PIN(pos_cmd_d);
HAL_PIN(pos_fb);
HAL_PIN(vel_fb);
HAL_PIN(current);
HAL_PIN(power);
HAL_PIN(temp);
HAL_PIN(vbus);
HAL_PIN(scale);
HAL_PIN(clock_scale);
HAL_PIN(in0);
HAL_PIN(in1);
HAL_PIN(in2);
HAL_PIN(in3);
HAL_PIN(fault);
HAL_PIN(out0);
HAL_PIN(out1);
HAL_PIN(out2);
HAL_PIN(out3);
HAL_PIN(enable);
HAL_PIN(index_clear);
HAL_PIN(index_out);
HAL_PIN(pos_advance);
HAL_PIN(syncs);
HAL_PIN(frames);

struct can_ctx_t {
  canp_cmd_t cmd;  // the newest command received
  int have_cmd;
  float since;     // [s] since the last SYNC
  int node;        // the filter's node
  int connected;
};

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct can_ctx_t *ctx      = (struct can_ctx_t *)ctx_ptr;
  struct can_pin_ctx_t *pins = (struct can_pin_ctx_t *)pin_ptr;
  PIN(node)        = 1;
  PIN(vel_scale)   = 100;
  PIN(timeout)     = 0.005;
  PIN(scale)       = 1;
  PIN(clock_scale) = 1;
  ctx->since       = 1.0;  // start in timeout
  ctx->node        = 0;
}

static void apply(struct can_pin_ctx_t *pins, const canp_cmd_t *c) {
  PIN(pos_cmd)   = c->pos;
  PIN(pos_cmd_d) = c->vel;
  PIN(enable)    = c->enable;
  PIN(index_out) = c->index_enable && PIN(index_clear) <= 0.0;
  PIN(out0)      = (c->out >> 0) & 1;
  PIN(out1)      = (c->out >> 1) & 1;
  PIN(out2)      = (c->out >> 2) & 1;
  PIN(out3)      = (c->out >> 3) & 1;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct can_ctx_t *ctx      = (struct can_ctx_t *)ctx_ptr;
  struct can_pin_ctx_t *pins = (struct can_pin_ctx_t *)pin_ptr;
  int node                   = (int)CLAMP(PIN(node), 1, CANP_NODE_MAX);
  float vs                   = MAX(PIN(vel_scale), 1e-3);
  if(node != ctx->node) {  // the controller's filters follow the node ID
    uint16_t ids[3] = {CANP_SYNC_ID, (uint16_t)CANP_CMD_ID(node), (uint16_t)CANP_DESC_REQ_ID(node)};
    can_hw_filter(ids, 3);
    ctx->node = node;
  }
  ctx->since += period;
  can_frame_t f;
  while(can_hw_recv(&f)) {
    PIN(frames) += 1;
    if(f.id == CANP_CMD_ID(node)) {
      if(!canp_cmd_decode(&f, &ctx->cmd, vs)) {
        ctx->have_cmd = 1;
      }
    } else if(f.id == CANP_DESC_REQ_ID(node) && f.dlc >= 2 && f.d[0] == CANP_DESC_READ) {
      // discovery: a block of this drive's description
      char text[96];
      int n = canp_desc_text(text, sizeof(text), (int)(vs * 1000.0f + 0.5f));
      can_frame_t out;
      canp_desc_chunk(&out, node, text, n + 1, f.d[1]);  // with the ending 0
      can_hw_send(&out);
    } else if(f.id == CANP_SYNC_ID && f.dlc == 1) {
      PIN(syncs) += 1;
      ctx->since = 0.0;
      if(!ctx->connected) {
        sscon_reset(&sscon0);  // a new link: the console starts afresh
      }
      ctx->connected = 1;
      if(ctx->have_cmd) {
        apply(pins, &ctx->cmd);
      }
      // the feedback answers this SYNC
      float slots[CANP_SLOTS];
      slots[CANP_SLOT_CURRENT] = PIN(current);
      slots[CANP_SLOT_POWER]   = PIN(power);
      slots[CANP_SLOT_TEMP]    = PIN(temp);
      slots[CANP_SLOT_VBUS]    = PIN(vbus);
      canp_fb_t fb             = {
          .pos   = PIN(pos_fb) + PIN(vel_fb) * PIN(pos_advance),
          .vel   = PIN(vel_fb),
          .fault = PIN(fault) > 0.0,
          .index = ctx->have_cmd && ctx->cmd.index_enable && PIN(index_clear) <= 0.0,
          .in    = (uint8_t)((PIN(in0) > 0) | (PIN(in1) > 0) << 1 | (PIN(in2) > 0) << 2 | (PIN(in3) > 0) << 3),
          .con   = sscon_frame(&sscon0, ctx->have_cmd ? ctx->cmd.con : 0),
      };
      can_frame_t out;
      canp_fb_encode(&out, node, &fb, vs, f.d[0], slots);
      can_hw_send(&out);
    }
  }
  if(ctx->since > MAX(PIN(timeout), period * 2)) {
    if(ctx->connected) {
      sscon_reset(&sscon0);
    }
    ctx->connected = 0;
    ctx->have_cmd  = 0;
    PIN(connected) = 0;
    PIN(error)     = 1;
    PIN(pos_cmd)   = 0;
    PIN(pos_cmd_d) = 0;
    PIN(out0)      = 0;
    PIN(out1)      = 0;
    PIN(out2)      = 0;
    PIN(out3)      = 0;
    PIN(enable)    = 0;
  } else {
    PIN(connected) = 1;
    PIN(error)     = 0;
  }
}

hal_comp_t can_comp_struct = {
    .name      = "can",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct can_ctx_t),
    .pin_count = sizeof(struct can_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
