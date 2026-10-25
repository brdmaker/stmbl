/*
 * The measurement bus (docs/vibsensor.md): vibration sensors and the
 * machine's PC on a CAN FD bus of their own, 1 Mbit/s arbitration, 5 Mbit/s
 * data. Header only, plain C: the sensor's firmware, the PC's side and the
 * host tests use the same code.
 *
 * IDs (11 bit; a lower ID wins the bus), node 1..15:
 *   0x010 + node   event, 8 B: crash, vibration level, temperature level
 *   0x050          machine state from the PC, 16 B, every 20 ms: time, rpm,
 *                  feed, teeth, flags. The sensors' order tracking takes
 *                  the rpm from it, and their reports its time.
 *   0x100 + node   features, 64 B, every interval (0.1 s): RMS, peak,
 *                  velocity and kurtosis per axis, bearing band, temperature
 *   0x140 + node   spectrum summary, 48 B, every FFT block: peak, orders,
 *                  tooth passing, chatter, envelope
 *   0x300 + node   spectrum, 64 B frames of 26 bins, on request or streamed
 *   0x400 + node   raw samples, 64 B frames of 9 x y z samples, streamed
 *   0x600 + node   console to the sensor: text (parameters: "vib0.fft_n = 4096")
 *   0x580 + node   console from the sensor
 * All values little endian; floats IEEE 754 single.
 */
#pragma once
#include <stdint.h>
#include <string.h>

#define VIBP_NODE_MAX 15
#define VIBP_EVENT_ID(node) (0x010 + (node))
#define VIBP_STATE_ID 0x050
#define VIBP_FEAT_ID(node) (0x100 + (node))
#define VIBP_SUM_ID(node) (0x140 + (node))
#define VIBP_SPEC_ID(node) (0x300 + (node))
#define VIBP_RAW_ID(node) (0x400 + (node))
#define VIBP_CON_TO_ID(node) (0x600 + (node))
#define VIBP_CON_FROM_ID(node) (0x580 + (node))

#define VIBP_SPEC_BINS 26  // per frame
#define VIBP_RAW_SAMPLES 9
#define VIBP_CON_TEXT 62   // console bytes per frame

typedef struct {
  uint16_t id;
  uint8_t len;  // 0..8, 12, 16, 20, 24, 32, 48, 64
  uint8_t d[64];
} vibp_frame_t;

// ---------------------------------------------------------------- payloads
enum { VIBP_EV_CRASH = 1, VIBP_EV_LEVEL = 2, VIBP_EV_TEMP = 3 };
typedef struct {
  uint8_t type, value;  // value: the level (0 ok, 1 warn, 2 alarm); a crash 1
  uint16_t seq;
  float x;              // crash: the shock (g); level: largest velocity (mm/s); temp: degC
} vibp_event_t;

enum { VIBP_ST_SPINDLE = 1, VIBP_ST_RUNNING = 2, VIBP_ST_TOOLCHANGE = 4 };
typedef struct {
  uint32_t time_ms;
  float rpm, feed;  // feed mm/min
  uint8_t teeth, flags;
  uint16_t tool;
} vibp_state_t;

typedef struct {
  uint32_t time_ms;
  uint16_t seq;
  uint8_t level, flags;  // flags: 0 crash latched, 1..2 temp_level
  float rms[3], peak[3], vel[3], kurt[3];
  float env, temp;
} vibp_feat_t;

typedef struct {
  uint32_t time_ms;
  uint16_t seq, fft_n;
  float fft_f, fft_a, ord[3], tpf, chat_f, chat_a, env_f, rpm;
} vibp_sum_t;

typedef struct {
  uint16_t seq, first, total;  // first bin of the frame, bins in the spectrum
  uint8_t axis;
  float scale;                 // g RMS of a bin at 65535
  uint16_t bin[VIBP_SPEC_BINS];
} vibp_spec_t;

typedef struct {
  uint32_t first;  // the first sample's number (counted since start)
  uint16_t flags;  // bit 0: the stream lost samples before this frame
  int16_t s[VIBP_RAW_SAMPLES][3];
} vibp_raw_t;

// ---------------------------------------------------------------- bytes
static inline void vibp_u16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
static inline void vibp_u32(uint8_t *p, uint32_t v) {
  vibp_u16(p, (uint16_t)v);
  vibp_u16(p + 2, (uint16_t)(v >> 16));
}
static inline void vibp_f32(uint8_t *p, float f) {
  uint32_t v;
  memcpy(&v, &f, 4);
  vibp_u32(p, v);
}
static inline uint16_t vibp_get16(const uint8_t *p) {
  return (uint16_t)(p[0] | p[1] << 8);
}
static inline uint32_t vibp_get32(const uint8_t *p) {
  return vibp_get16(p) | (uint32_t)vibp_get16(p + 2) << 16;
}
static inline float vibp_getf(const uint8_t *p) {
  uint32_t v = vibp_get32(p);
  float f;
  memcpy(&f, &v, 4);
  return f;
}
static inline void vibp_fs(uint8_t *p, const float *f, int n) {
  for(int i = 0; i < n; i++) {
    vibp_f32(p + 4 * i, f[i]);
  }
}
static inline void vibp_gfs(const uint8_t *p, float *f, int n) {
  for(int i = 0; i < n; i++) {
    f[i] = vibp_getf(p + 4 * i);
  }
}

// ---------------------------------------------------------------- frames
static inline void vibp_put_event(vibp_frame_t *f, int node, const vibp_event_t *e) {
  f->id   = VIBP_EVENT_ID(node);
  f->len  = 8;
  f->d[0] = e->type;
  f->d[1] = e->value;
  vibp_u16(f->d + 2, e->seq);
  vibp_f32(f->d + 4, e->x);
}
static inline void vibp_get_event(const vibp_frame_t *f, vibp_event_t *e) {
  e->type  = f->d[0];
  e->value = f->d[1];
  e->seq   = vibp_get16(f->d + 2);
  e->x     = vibp_getf(f->d + 4);
}

static inline void vibp_put_state(vibp_frame_t *f, const vibp_state_t *s) {
  f->id  = VIBP_STATE_ID;
  f->len = 16;
  vibp_u32(f->d, s->time_ms);
  vibp_f32(f->d + 4, s->rpm);
  vibp_f32(f->d + 8, s->feed);
  f->d[12] = s->teeth;
  f->d[13] = s->flags;
  vibp_u16(f->d + 14, s->tool);
}
static inline void vibp_get_state(const vibp_frame_t *f, vibp_state_t *s) {
  s->time_ms = vibp_get32(f->d);
  s->rpm     = vibp_getf(f->d + 4);
  s->feed    = vibp_getf(f->d + 8);
  s->teeth   = f->d[12];
  s->flags   = f->d[13];
  s->tool    = vibp_get16(f->d + 14);
}

static inline void vibp_put_feat(vibp_frame_t *f, int node, const vibp_feat_t *v) {
  f->id  = VIBP_FEAT_ID(node);
  f->len = 64;
  vibp_u32(f->d, v->time_ms);
  vibp_u16(f->d + 4, v->seq);
  f->d[6] = v->level;
  f->d[7] = v->flags;
  vibp_fs(f->d + 8, v->rms, 3);
  vibp_fs(f->d + 20, v->peak, 3);
  vibp_fs(f->d + 32, v->vel, 3);
  vibp_fs(f->d + 44, v->kurt, 3);
  vibp_f32(f->d + 56, v->env);
  vibp_f32(f->d + 60, v->temp);
}
static inline void vibp_get_feat(const vibp_frame_t *f, vibp_feat_t *v) {
  v->time_ms = vibp_get32(f->d);
  v->seq     = vibp_get16(f->d + 4);
  v->level   = f->d[6];
  v->flags   = f->d[7];
  vibp_gfs(f->d + 8, v->rms, 3);
  vibp_gfs(f->d + 20, v->peak, 3);
  vibp_gfs(f->d + 32, v->vel, 3);
  vibp_gfs(f->d + 44, v->kurt, 3);
  v->env  = vibp_getf(f->d + 56);
  v->temp = vibp_getf(f->d + 60);
}

static inline void vibp_put_sum(vibp_frame_t *f, int node, const vibp_sum_t *v) {
  f->id  = VIBP_SUM_ID(node);
  f->len = 48;
  vibp_u32(f->d, v->time_ms);
  vibp_u16(f->d + 4, v->seq);
  vibp_u16(f->d + 6, v->fft_n);
  float x[10] = {v->fft_f, v->fft_a, v->ord[0], v->ord[1], v->ord[2], v->tpf, v->chat_f, v->chat_a, v->env_f, v->rpm};
  vibp_fs(f->d + 8, x, 10);
}
static inline void vibp_get_sum(const vibp_frame_t *f, vibp_sum_t *v) {
  float x[10];
  v->time_ms = vibp_get32(f->d);
  v->seq     = vibp_get16(f->d + 4);
  v->fft_n   = vibp_get16(f->d + 6);
  vibp_gfs(f->d + 8, x, 10);
  v->fft_f  = x[0];
  v->fft_a  = x[1];
  v->ord[0] = x[2];
  v->ord[1] = x[3];
  v->ord[2] = x[4];
  v->tpf    = x[5];
  v->chat_f = x[6];
  v->chat_a = x[7];
  v->env_f  = x[8];
  v->rpm    = x[9];
}

static inline void vibp_put_spec(vibp_frame_t *f, int node, const vibp_spec_t *s) {
  f->id  = VIBP_SPEC_ID(node);
  f->len = 64;
  vibp_u16(f->d, s->seq);
  vibp_u16(f->d + 2, s->first);
  vibp_u16(f->d + 4, s->total);
  f->d[6] = s->axis;
  f->d[7] = 0;
  vibp_f32(f->d + 8, s->scale);
  for(int i = 0; i < VIBP_SPEC_BINS; i++) {
    vibp_u16(f->d + 12 + 2 * i, s->bin[i]);
  }
}
static inline void vibp_get_spec(const vibp_frame_t *f, vibp_spec_t *s) {
  s->seq   = vibp_get16(f->d);
  s->first = vibp_get16(f->d + 2);
  s->total = vibp_get16(f->d + 4);
  s->axis  = f->d[6];
  s->scale = vibp_getf(f->d + 8);
  for(int i = 0; i < VIBP_SPEC_BINS; i++) {
    s->bin[i] = vibp_get16(f->d + 12 + 2 * i);
  }
}
// A spectrum (bin RMS values, g) as frames: the bins scaled to the largest.
// Returns the number of frames; out holds at least (total + 25) / 26.
static inline int vibp_spec_frames(vibp_frame_t *out, int node, uint16_t seq, uint8_t axis, const float *rms, int total) {
  float mx = 0.0f;
  for(int k = 0; k < total; k++) {
    mx = rms[k] > mx ? rms[k] : mx;
  }
  int nf = 0;
  for(int first = 0; first < total; first += VIBP_SPEC_BINS, nf++) {
    vibp_spec_t s = {seq, (uint16_t)first, (uint16_t)total, axis, mx, {0}};
    for(int i = 0; i < VIBP_SPEC_BINS && first + i < total; i++) {
      s.bin[i] = mx > 0.0f ? (uint16_t)(rms[first + i] / mx * 65535.0f + 0.5f) : 0;
    }
    vibp_put_spec(&out[nf], node, &s);
  }
  return nf;
}

static inline void vibp_put_raw(vibp_frame_t *f, int node, const vibp_raw_t *r) {
  f->id  = VIBP_RAW_ID(node);
  f->len = 64;
  memset(f->d, 0, 64);
  vibp_u32(f->d, r->first);
  vibp_u16(f->d + 4, r->flags);
  for(int i = 0; i < VIBP_RAW_SAMPLES; i++) {
    for(int a = 0; a < 3; a++) {
      vibp_u16(f->d + 6 + 6 * i + 2 * a, (uint16_t)r->s[i][a]);
    }
  }
}
static inline void vibp_get_raw(const vibp_frame_t *f, vibp_raw_t *r) {
  r->first = vibp_get32(f->d);
  r->flags = vibp_get16(f->d + 4);
  for(int i = 0; i < VIBP_RAW_SAMPLES; i++) {
    for(int a = 0; a < 3; a++) {
      r->s[i][a] = (int16_t)vibp_get16(f->d + 6 + 6 * i + 2 * a);
    }
  }
}

// Console text: [seq, n, n bytes of text]; the length rounded up to a CAN FD length.
static inline uint8_t vibp_fd_len(int n) {
  static const uint8_t lens[] = {8, 12, 16, 20, 24, 32, 48, 64};
  if(n <= 8) {
    return (uint8_t)(n < 0 ? 0 : n);
  }
  for(unsigned i = 0; i < sizeof(lens); i++) {
    if(n <= lens[i]) {
      return lens[i];
    }
  }
  return 64;
}
static inline int vibp_put_con(vibp_frame_t *f, uint16_t id, uint8_t seq, const char *text, int n) {
  n      = n > VIBP_CON_TEXT ? VIBP_CON_TEXT : n;
  f->id  = id;
  f->len = vibp_fd_len(n + 2);
  memset(f->d, 0, f->len);
  f->d[0] = seq;
  f->d[1] = (uint8_t)n;
  memcpy(f->d + 2, text, n);
  return n;
}

// ---------------------------------------------------------------- bus time
// A frame's time on the bus (us), CAN FD with bit rate switch: the
// arbitration and the end at arb_kbps, control, data and CRC at data_kbps,
// with stuff bits (about one in ten, plus the CRC's fixed ones). Classic
// CAN: data_kbps = 0.
static inline float vibp_frame_us(int len, float arb_kbps, float data_kbps) {
  if(data_kbps <= 0.0f) {  // classic: 47 bits + data, ~10 % stuffing
    return (47.0f + 8.0f * len) * 1.1f / arb_kbps * 1000.0f;
  }
  float arb  = 30.0f;  // SOF, ID, RRS, IDE, FDF, res, BRS; CRC delimiter, ACK, EOF, intermission
  float crc  = len > 16 ? 21.0f + 6.0f : 17.0f + 5.0f;  // CRC and its fixed stuff bits
  float data = (1.0f + 4.0f + 4.0f + 8.0f * len) * 1.1f + crc;  // ESI, DLC, stuff count
  return (arb / arb_kbps + data / data_kbps) * 1000.0f;
}
