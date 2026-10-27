#include "vnode_comp.h"
#include "commands.h"
#include "hal.h"
#include "defines.h"
#include "vibaddr.h"
#include "vibdsp.h"
#include "vibproto.h"
#include "vnode_hw.h"
#include "version_number.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// The vibration sensor on its measurement bus (docs/vibsensor.md §3): what
// vib0 finds goes out as frames, the PC's machine state comes in, and so do
// the node number (shared/vibaddr.h) and the console.
//   out:  features (0x100 + node) at every report, the summary (0x140 +
//         node) at every FFT, events (0x010 + node) when crash, level or
//         temp_level change, a tap while the PC identifies; the spectrum
//         (0x300 + node) on the console command vnspec
//   in:   the machine state (0x050): rpm, feed, teeth, tool, flags; rpm
//         goes to 0 when it is older than 0.5 s, so the orders aren't read
//         at a stale speed
//         console lines (0x600 + node), answered on 0x580 + node
// Without a node number (0) the sensor sends nothing but its answers to
// the PC's numbering (vibaddr).
// Its inputs link to vib0 (vibsensor/conf/vibsensor.txt). One instance.

HAL_COMP(vnode);

// from vib0
HAL_PIN(rms_x);
HAL_PIN(rms_y);
HAL_PIN(rms_z);
HAL_PIN(peak_x);
HAL_PIN(peak_y);
HAL_PIN(peak_z);
HAL_PIN(vel_x);
HAL_PIN(vel_y);
HAL_PIN(vel_z);
HAL_PIN(kurt_x);
HAL_PIN(kurt_y);
HAL_PIN(kurt_z);
HAL_PIN(env);
HAL_PIN(env_f);
HAL_PIN(temp);
HAL_PIN(level);
HAL_PIN(temp_level);
HAL_PIN(crash);
HAL_PIN(shock);
HAL_PIN(taps);
HAL_PIN(reports);
HAL_PIN(ffts);
HAL_PIN(fft_n);
HAL_PIN(fft_axis);
HAL_PIN(fft_f);
HAL_PIN(fft_a);
HAL_PIN(ord1);
HAL_PIN(ord2);
HAL_PIN(ord3);
HAL_PIN(tpf);
HAL_PIN(chat_f);
HAL_PIN(chat_a);
// from the machine state, to vib0
HAL_PIN(rpm);
HAL_PIN(feed);
HAL_PIN(teeth);
HAL_PIN(tool);
HAL_PIN(spindle);
HAL_PIN(running);
HAL_PIN(state_age);  // s since the last machine state
// the node
HAL_PIN(node);
HAL_PIN(tx);
HAL_PIN(rx);
HAL_PIN(tx_full);

struct vnode_ctx_t {
  vibaddr_t a;
  float reports, ffts, taps, level, temp_level, crash;
  uint16_t seq_feat, seq_sum, seq_ev, seq_spec;
  uint64_t state_ms;
  int have_state;
  char line[128];
  int line_n;
  uint8_t con_seq;
};

extern volatile uint64_t systime;
static struct vnode_ctx_t *g_ctx;
static struct vnode_pin_ctx_t *g_pins;

static void send(struct vnode_pin_ctx_t *pins, const vibp_frame_t *f) {
  if(vnode_hw_send(f) < 0) {
    PIN(tx_full)++;
  } else {
    PIN(tx)++;
  }
}

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct vnode_ctx_t *ctx      = (struct vnode_ctx_t *)ctx_ptr;
  struct vnode_pin_ctx_t *pins = (struct vnode_pin_ctx_t *)pin_ptr;
  memset(ctx, 0, sizeof(*ctx));
  vnode_hw_uid(ctx->a.uid);
  ctx->a.fw = STMBL_FW_VERSION;
  if(!vnode_hw_load(&ctx->a.node, ctx->a.name) || ctx->a.node > VIBP_NODE_MAX) {
    ctx->a.node = 0;
    memset(ctx->a.name, 0, sizeof(ctx->a.name));
  }
  PIN(node)  = ctx->a.node;
  PIN(teeth) = 2;
  ctx->reports = ctx->ffts = ctx->taps = -1.0f;  // the first values seen are no news
  g_ctx                                = ctx;
  g_pins                               = pins;
}

static void event(struct vnode_ctx_t *ctx, struct vnode_pin_ctx_t *pins, int type, int value, float x) {
  vibp_event_t e = {(uint8_t)type, (uint8_t)value, ctx->seq_ev++, x};
  vibp_frame_t f;
  vibp_put_event(&f, ctx->a.node, &e);
  send(pins, &f);
}

static void console_in(struct vnode_ctx_t *ctx, const vibp_frame_t *f) {
  int n = f->len >= 2 ? f->d[1] : 0;
  n     = MIN(n, f->len - 2);
  for(int i = 0; i < n; i++) {
    char c = (char)f->d[2 + i];
    if(c == '\n' || c == '\r') {
      if(ctx->line_n) {
        ctx->line[ctx->line_n] = 0;
        ctx->line_n            = 0;
        hal_parse(ctx->line);
      }
    } else if(ctx->line_n < (int)sizeof(ctx->line) - 1) {
      ctx->line[ctx->line_n++] = c;
    }
  }
}

static void nrt_func(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct vnode_ctx_t *ctx      = (struct vnode_ctx_t *)ctx_ptr;
  struct vnode_pin_ctx_t *pins = (struct vnode_pin_ctx_t *)pin_ptr;
  vibp_frame_t f, out;
  uint32_t now = (uint32_t)systime;

  for(int i = 0; i < 16 && vnode_hw_recv(&f); i++) {  // a bounded share of the loop
    PIN(rx)++;
    if(f.id == VIBA_CMD_ID) {
      if(vibaddr_rx(&ctx->a, &f, &out)) {
        send(pins, &out);
      }
      if(ctx->a.store_req) {
        ctx->a.store_req = 0;
        vnode_hw_store(ctx->a.node, ctx->a.name);
      }
      PIN(node) = ctx->a.node;
    } else if(f.id == VIBP_STATE_ID && f.len >= 16) {
      vibp_state_t s;
      vibp_get_state(&f, &s);
      PIN(rpm)        = s.rpm;
      PIN(feed)       = s.feed;
      PIN(teeth)      = s.teeth;
      PIN(tool)       = s.tool;
      PIN(spindle)    = (s.flags & VIBP_ST_SPINDLE) != 0;
      PIN(running)    = (s.flags & VIBP_ST_RUNNING) != 0;
      ctx->state_ms   = systime;
      ctx->have_state = 1;
    } else if(ctx->a.node && f.id == VIBP_CON_TO_ID(ctx->a.node)) {
      console_in(ctx, &f);
    }
  }
  PIN(state_age) = ctx->have_state ? (systime - ctx->state_ms) / 1000.0f : -1.0f;
  if(PIN(state_age) > 0.5f) {
    PIN(rpm) = 0;  // stale: no order tracking at an old speed
  }

  int node = ctx->a.node;
  if(PIN(taps) != ctx->taps) {
    if(ctx->taps >= 0.0f && vibaddr_tap(&ctx->a, &out)) {
      send(pins, &out);  // even without a number: this is how one is found
    }
    ctx->taps = PIN(taps);
  }
  if(!node) {
    ctx->reports    = PIN(reports);
    ctx->ffts       = PIN(ffts);
    ctx->crash = ctx->level = ctx->temp_level = 0;  // once numbered, it says what is not 0
    return;
  }

  if(PIN(crash) > 0.0f && ctx->crash <= 0.0f) {
    event(ctx, pins, VIBP_EV_CRASH, 1, PIN(shock));
  }
  ctx->crash = PIN(crash);
  if(PIN(level) != ctx->level) {
    event(ctx, pins, VIBP_EV_LEVEL, (int)PIN(level), MAX3(PIN(vel_x), PIN(vel_y), PIN(vel_z)));
    ctx->level = PIN(level);
  }
  if(PIN(temp_level) != ctx->temp_level) {
    event(ctx, pins, VIBP_EV_TEMP, (int)PIN(temp_level), PIN(temp));
    ctx->temp_level = PIN(temp_level);
  }

  if(PIN(reports) != ctx->reports) {
    ctx->reports  = PIN(reports);
    vibp_feat_t v = {now, ctx->seq_feat++, (uint8_t)PIN(level), (uint8_t)((PIN(crash) > 0.0f) | ((int)PIN(temp_level) << 1)),
                     {PIN(rms_x), PIN(rms_y), PIN(rms_z)}, {PIN(peak_x), PIN(peak_y), PIN(peak_z)},
                     {PIN(vel_x), PIN(vel_y), PIN(vel_z)}, {PIN(kurt_x), PIN(kurt_y), PIN(kurt_z)},
                     PIN(env), PIN(temp)};
    vibp_put_feat(&f, node, &v);
    send(pins, &f);
  }
  if(PIN(ffts) != ctx->ffts) {
    ctx->ffts    = PIN(ffts);
    vibp_sum_t s = {now, ctx->seq_sum++, (uint16_t)PIN(fft_n), PIN(fft_f), PIN(fft_a), {PIN(ord1), PIN(ord2), PIN(ord3)},
                    PIN(tpf), PIN(chat_f), PIN(chat_a), PIN(env_f), PIN(rpm)};
    vibp_put_sum(&f, node, &s);
    send(pins, &f);
  }

  // the console's output: a frame at a time, up to 4 per pass
  for(int i = 0; i < 4; i++) {
    char buf[VIBP_CON_TEXT];
    int n = vnode_hw_con_read(buf, sizeof(buf));
    if(n <= 0) {
      break;
    }
    vibp_put_con(&f, VIBP_CON_FROM_ID(node), ctx->con_seq++, buf, n);
    send(pins, &f);
  }
}

const vib_spec_t *vib_last_spectrum(void);  // shared/comps/vib.c

// vnspec: the last spectrum (bin RMS, g) out as frames, 26 bins each,
// scaled to the largest (as vibp_spec_frames, without the arrays)
void vnspec(char *ptr) {
  (void)ptr;
  const vib_spec_t *s = vib_last_spectrum();
  if(!g_ctx || !g_ctx->a.node || !s) {
    printf("vnspec: no %s\n", s ? "node number" : "spectrum yet");
    return;
  }
  int total = s->n / 2 + 1, nf = 0;
  float mx  = 0.0f;
  for(int k = 0; k < total; k++) {
    mx = MAX(mx, vib_bin_rms(s, k));
  }
  for(int first = 0; first < total; first += VIBP_SPEC_BINS, nf++) {
    vibp_spec_t sp = {g_ctx->seq_spec, (uint16_t)first, (uint16_t)total, (uint8_t)g_pins->fft_axis.source->value, mx, {0}};
    for(int i = 0; i < VIBP_SPEC_BINS && first + i < total; i++) {
      sp.bin[i] = mx > 0.0f ? (uint16_t)(vib_bin_rms(s, first + i) / mx * 65535.0f + 0.5f) : 0;
    }
    vibp_frame_t f;
    vibp_put_spec(&f, g_ctx->a.node, &sp);
    send(g_pins, &f);
  }
  g_ctx->seq_spec++;
  printf("vnspec: %d bins in %d frames\n", total, nf);
}
COMMAND("vnspec", vnspec, "send the last vibration spectrum on the bus");

// vnid: the node's identity
void vnid(char *ptr) {
  (void)ptr;
  if(!g_ctx) {
    return;
  }
  printf("vnid uid ");
  for(int i = 0; i < VIBA_UID; i++) {
    printf("%02x", g_ctx->a.uid[i]);
  }
  printf(" node %d name %.16s fw %d\n", g_ctx->a.node, g_ctx->a.name, g_ctx->a.fw);
}
COMMAND("vnid", vnid, "the sensor's unique ID, node number and name");

hal_comp_t vnode_comp_struct = {
    .name      = "vnode",
    .nrt       = nrt_func,
    .rt        = 0,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct vnode_ctx_t),
    .pin_count = sizeof(struct vnode_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
