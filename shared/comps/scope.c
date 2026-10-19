#include "scope_comp.h"
#include "commands.h"
#include "hal.h"
#include "defines.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Capture scope in the drive: records up to 8 pins into its own memory at
// the full loop rate, triggered, for download over a slow console (the
// smart-serial console, ~1 kB/s; USB too). Instead of streaming in real
// time (term), it captures first and sends afterwards.
//
//   scope0.ch0 = curpid0.iq_fb         channels: link any pins (nch of them)
//   scope0.nch = 2                     1..8; memory 4096 samples: depth = 4096 / nch
//   scope0.div = 1                     a sample every div periods
//   scope0.fast = 0                    0: the rt loop (5 kHz), 1: the frt loop
//                                      (20 kHz; set scope0.frt_prio then)
//   scope0.trig = fault0.state         trigger source (link), its level and
//   scope0.level = 0.5 / edge = 0      edge: 0 rising, 1 falling, 2 either
//   scope0.pre = 0.1                   share of the capture before the trigger
//   scope0.mode = 0                    0 single, 1 normal (re-armed after each
//                                      dump), 2 auto (as normal, and forced if
//                                      no trigger within a capture length)
//   scope0.arm = 1                     a rising edge arms (also: scopearm);
//                                      armed at start this way, the scope
//                                      catches e.g. the first fault, unattended
//   scope0.state                       0 idle, 1 armed, 2 triggered, 3 done
// Commands: scopearm, scopetrig (force), scopestat, scopedump (the capture:
// per channel its source and range, the samples as 16 bit in base64 lines
// with a CRC-16 each; servoterm-cli --capture turns it into CSV).

HAL_COMP(scope);

HAL_PINA(ch, 8);
HAL_PIN(nch);
HAL_PIN(div);
HAL_PIN(fast);
HAL_PIN(trig);
HAL_PIN(level);
HAL_PIN(edge);
HAL_PIN(pre);
HAL_PIN(mode);
HAL_PIN(arm);
HAL_PIN(state);
HAL_PIN(depth);
HAL_PIN(dt);

#ifndef SCOPE_MEM
#define SCOPE_MEM 4096  // samples in all; a target may set less (-DSCOPE_MEM=...)
#endif
enum { S_IDLE, S_ARMED, S_TRIG, S_DONE };

static float scope_buf[SCOPE_MEM];

struct scope_ctx_t {
  uint32_t nch, depth, wp, count, post, tpos, divc;
  uint32_t armed_frames;
  float prev_trig, last_arm, dt;
  volatile int force, arm_cmd;
};

static struct scope_ctx_t *g_ctx;
static struct scope_pin_ctx_t *g_pins;

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct scope_ctx_t *ctx      = (struct scope_ctx_t *)ctx_ptr;
  struct scope_pin_ctx_t *pins = (struct scope_pin_ctx_t *)pin_ptr;
  PIN(nch)   = 4;
  PIN(div)   = 1;
  PIN(level) = 0.5;
  PIN(pre)   = 0.1;
  g_ctx      = ctx;
  g_pins     = pins;
}

static void do_arm(struct scope_ctx_t *ctx, struct scope_pin_ctx_t *pins, float period) {
  ctx->nch   = (uint32_t)CLAMP(PIN(nch), 1, 8);
  ctx->depth = SCOPE_MEM / ctx->nch;
  ctx->post  = ctx->depth - (uint32_t)(CLAMP(PIN(pre), 0.0, 1.0) * ctx->depth);
  if(ctx->post < 1) {
    ctx->post = 1;  // pre = 1: the trigger sample is the last one, still in the capture
  }
  ctx->wp = ctx->count = ctx->divc = ctx->armed_frames = 0;
  ctx->dt         = period * MAX(PIN(div), 1.0);
  ctx->prev_trig  = PIN(trig);
  ctx->force      = 0;
  PIN(depth)      = ctx->depth;
  PIN(dt)         = ctx->dt;
  PIN(state)      = S_ARMED;
}

static void sample(struct scope_ctx_t *ctx, struct scope_pin_ctx_t *pins, float period) {
  if(PIN(arm) > 0.0 && ctx->last_arm <= 0.0) {
    ctx->arm_cmd = 1;
  }
  ctx->last_arm = PIN(arm);
  if(ctx->arm_cmd) {
    ctx->arm_cmd = 0;
    do_arm(ctx, pins, period);
  }
  int st = (int)PIN(state);
  if(st != S_ARMED && st != S_TRIG) {
    return;
  }
  if(++ctx->divc < (uint32_t)MAX(PIN(div), 1.0)) {
    return;
  }
  ctx->divc = 0;
  float *f = &scope_buf[ctx->wp * ctx->nch];
  for(uint32_t i = 0; i < ctx->nch; i++) {
    f[i] = PINA(ch, i);
  }
  ctx->wp = (ctx->wp + 1) % ctx->depth;
  ctx->count++;
  float v = PIN(trig), l = PIN(level);
  if(st == S_ARMED) {
    uint32_t pre = ctx->depth - ctx->post;
    int e = (int)PIN(edge);
    int hit = (e != 1 && ctx->prev_trig < l && v >= l) || (e != 0 && ctx->prev_trig > l && v <= l);
    ctx->prev_trig = v;
    if(ctx->count >= pre) {
      ctx->armed_frames++;
      if(PIN(mode) == 2 && ctx->armed_frames > ctx->depth) {
        ctx->force = 1;
      }
      if(hit || ctx->force) {
        ctx->tpos  = ctx->count - 1;  // the trigger sample
        ctx->count = 1;               // counts post samples from here
        PIN(state) = S_TRIG;
        if(ctx->post <= 1) {
          PIN(state) = S_DONE;
        }
      }
    }
    return;
  }
  if(ctx->count >= ctx->post) {
    PIN(state) = S_DONE;
  }
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct scope_pin_ctx_t *pins = (struct scope_pin_ctx_t *)pin_ptr;
  if(PIN(fast) <= 0.0) {
    sample((struct scope_ctx_t *)ctx_ptr, pins, period);
  }
}

static void frt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct scope_pin_ctx_t *pins = (struct scope_pin_ctx_t *)pin_ptr;
  if(PIN(fast) > 0.0) {
    sample((struct scope_ctx_t *)ctx_ptr, pins, period);
  }
}

void scope_arm(char *ptr) {
  (void)ptr;
  if(g_ctx) {
    g_ctx->arm_cmd = 1;
    printf("scope: armed\n");
  }
}
COMMAND("scopearm", scope_arm, "arm the capture scope");

void scope_trig(char *ptr) {
  (void)ptr;
  if(g_ctx) {
    g_ctx->force = 1;
  }
}
COMMAND("scopetrig", scope_trig, "force the capture scope's trigger");

static const char *state_name[] = {"idle", "armed", "triggered", "done"};

void scope_stat(char *ptr) {
  (void)ptr;
  if(!g_ctx) {
    printf("scope: not loaded\n");
    return;
  }
  struct scope_pin_ctx_t *pins = g_pins;
  int st                       = (int)CLAMP(PIN(state), 0, 3);
  printf("scope: %s, %lu channels x %lu samples, dt %g s\n", state_name[st], (unsigned long)g_ctx->nch,
         (unsigned long)g_ctx->depth, (double)g_ctx->dt);
}
COMMAND("scopestat", scope_stat, "capture scope state");

static uint16_t crc16(const uint8_t *d, int n) {  // CRC-16/CCITT-FALSE
  uint16_t c = 0xffff;
  for(int i = 0; i < n; i++) {
    c ^= (uint16_t)d[i] << 8;
    for(int b = 0; b < 8; b++) {
      c = c & 0x8000 ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
  }
  return c;
}

static void b64(const uint8_t *in, int n, char *out) {
  static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int o = 0;
  for(int i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)in[i] << 16 | (i + 1 < n ? (uint32_t)in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
    out[o++]   = t[(v >> 18) & 63];
    out[o++]   = t[(v >> 12) & 63];
    out[o++]   = i + 1 < n ? t[(v >> 6) & 63] : '=';
    out[o++]   = i + 2 < n ? t[v & 63] : '=';
  }
  out[o] = 0;
}

void scope_dump(char *ptr) {
  (void)ptr;
  if(!g_ctx) {
    printf("scope: not loaded\n");
    return;
  }
  struct scope_ctx_t *ctx      = g_ctx;
  struct scope_pin_ctx_t *pins = g_pins;
  if((int)PIN(state) != S_DONE) {
    printf("scope: no capture (%s)\n", state_name[(int)CLAMP(PIN(state), 0, 3)]);
    return;
  }
  uint32_t n = ctx->depth, nch = ctx->nch;
  // the oldest sample is at wp (the ring is full: pre + post = depth)
  uint32_t first = ctx->wp;
  uint32_t pre   = ctx->depth - ctx->post;
  float mn[8], mx[8];
  for(uint32_t c = 0; c < nch; c++) {
    mn[c] = mx[c] = scope_buf[c];
    for(uint32_t i = 0; i < n; i++) {
      float v = scope_buf[i * nch + c];
      mn[c]   = v < mn[c] ? v : mn[c];
      mx[c]   = v > mx[c] ? v : mx[c];
    }
  }
  printf("scope begin nch=%lu n=%lu dt=%g pre=%lu\n", (unsigned long)nch, (unsigned long)n, (double)ctx->dt,
         (unsigned long)pre);
  for(uint32_t c = 0; c < nch; c++) {
    hal_pin_inst_t *src = pins->ch[c].source;
    hal_comp_inst_t *ci = comp_inst_by_pin_inst(src);
    char name[64];
    if(src && ci && src != &pins->ch[c]) {
      snprintf(name, sizeof(name), "%s%lu.%s", ci->comp->name, (unsigned long)ci->instance,
               (char *)pin_by_pin_inst(src));
    } else {
      snprintf(name, sizeof(name), "scope0.ch%lu", (unsigned long)c);
    }
    printf("scope ch%lu %s min=%.9g max=%.9g\n", (unsigned long)c, name, (double)mn[c], (double)mx[c]);
  }
  // 16 bit per sample, time major, channels interleaved; 24 samples a line
  uint8_t raw[48];
  char line[72];
  int k = 0, lines = 0;
  for(uint32_t i = 0; i < n; i++) {
    uint32_t idx = (first + i) % n;
    for(uint32_t c = 0; c < nch; c++) {
      float v     = scope_buf[idx * nch + c];
      float span  = mx[c] - mn[c];
      uint16_t q  = span > 0.0f ? (uint16_t)((v - mn[c]) / span * 65535.0f + 0.5f) : 0;
      raw[k++]    = q & 0xff;
      raw[k++]    = q >> 8;
      if(k == sizeof(raw) || (i == n - 1 && c == nch - 1)) {
        b64(raw, k, line);
        printf("scope data %s %04x\n", line, crc16(raw, k));
        k = 0;
        lines++;
      }
    }
  }
  printf("scope end lines=%d\n", lines);
  if(PIN(mode) >= 1) {  // normal and auto: again
    ctx->arm_cmd = 1;
  }
}
COMMAND("scopedump", scope_dump, "download the capture scope's samples");

hal_comp_t scope_comp_struct = {
    .name      = "scope",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = frt_func,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct scope_ctx_t),
    .pin_count = sizeof(struct scope_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
