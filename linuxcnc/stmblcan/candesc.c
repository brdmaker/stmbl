/*
 * Device descriptions: see candesc.h.
 */
#include "candesc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(char *err, int errlen, const char *msg, const char *tok) {
  snprintf(err, (size_t)errlen, "%s%s%s", msg, tok ? ": " : "", tok ? tok : "");
  return -1;
}

// "0x1A0", "416", "0x180+node"
static int parse_id(const char *s, int node, uint16_t *id) {
  char *end;
  long v = strtol(s, &end, 0);
  if(end == s) {
    return -1;
  }
  if(!strcmp(end, "+node")) {
    v += node;
  } else if(*end) {
    return -1;
  }
  if(v < 0 || v > 0x7FF) {
    return -1;
  }
  *id = (uint16_t)v;
  return 0;
}

static int parse_type(const char *s, uint8_t *type, uint8_t *len) {
  if(!strcmp(s, "bit")) {
    *type = CD_BIT, *len = 1;
    return 0;
  }
  if(!strcmp(s, "f32")) {
    *type = CD_F32, *len = 32;
    return 0;
  }
  if((s[0] == 'u' || s[0] == 's') && s[1]) {
    char *end;
    long n = strtol(s + 1, &end, 10);
    if(*end || n < (s[0] == 's' ? 2 : 1) || n > 32) {
      return -1;
    }
    *type = s[0] == 'u' ? CD_UINT : CD_SINT;
    *len  = (uint8_t)n;
    return 0;
  }
  return -1;
}

int candesc_parse(candesc_t *d, const char *text, char *err, int errlen) {
  memset(d, 0, sizeof(*d));
  d->vel_scale = 100.0f;
  int lineno = 0;
  const char *p = text;
  candesc_frame_t *cur = 0;
  // pending "+node" IDs are resolved at the end, when the node is known
  char idtext[CANDESC_FRAMES][32];
  while(*p) {
    char line[128];
    const char *nl = strchr(p, '\n');
    size_t n       = nl ? (size_t)(nl - p) : strlen(p);
    lineno++;
    if(n >= sizeof(line)) {
      fail(err, errlen, "line too long", 0);
      return lineno;
    }
    memcpy(line, p, n);
    line[n] = 0;
    p += n + (nl ? 1 : 0);
    char *hash = strchr(line, '#');
    if(hash) {
      *hash = 0;
    }
    char *tok[10];
    int nt = 0;
    for(char *s = strtok(line, " \t\r"); s && nt < 10; s = strtok(NULL, " \t\r")) {
      tok[nt++] = s;
    }
    if(nt == 0) {
      continue;
    }
    if(!strcmp(tok[0], "device") && nt == 2) {
      snprintf(d->name, sizeof(d->name), "%s", tok[1]);
    } else if(!strcmp(tok[0], "node") && nt == 2) {
      d->node = atoi(tok[1]);
      if(d->node < 1 || d->node > CANP_NODE_MAX) {
        fail(err, errlen, "node: 1..127", tok[1]);
        return lineno;
      }
    } else if(!strcmp(tok[0], "profile") && nt == 2) {
      if(strcmp(tok[1], "stmbl1")) {
        fail(err, errlen, "unknown profile", tok[1]);
        return lineno;
      }
      d->stmbl = 1;
    } else if(!strcmp(tok[0], "vel_scale") && nt == 2) {
      d->vel_scale = (float)atof(tok[1]);
    } else if(!strcmp(tok[0], "firmware") || !strcmp(tok[0], "unit")) {
      // informational
    } else if(!strcmp(tok[0], "frame") && (nt == 5 || nt == 7)) {
      if(d->nframes >= CANDESC_FRAMES) {
        fail(err, errlen, "too many frames", 0);
        return lineno;
      }
      cur = &d->frame[d->nframes];
      snprintf(cur->name, sizeof(cur->name), "%s", tok[1]);
      snprintf(idtext[d->nframes], sizeof(idtext[0]), "%s", tok[2]);
      int dlc = atoi(tok[3]);
      if(dlc < 1 || dlc > 8) {
        fail(err, errlen, "frame length: 1..8", tok[3]);
        return lineno;
      }
      cur->dlc = (uint8_t)dlc;
      if(!strcmp(tok[4], "rx")) {
        cur->dir = CD_RX;
      } else if(!strcmp(tok[4], "tx")) {
        cur->dir = CD_TX;
      } else {
        fail(err, errlen, "frame direction: rx or tx", tok[4]);
        return lineno;
      }
      cur->every = 1;
      if(nt == 7) {
        if(strcmp(tok[5], "every") || atoi(tok[6]) < 1 || atoi(tok[6]) > 100) {
          fail(err, errlen, "expected: every N (1..100)", tok[5]);
          return lineno;
        }
        cur->every = (uint8_t)atoi(tok[6]);
      }
      d->nframes++;
    } else if(cur && nt >= 3 && tok[0][0] != 0) {
      if(cur->nfields >= CANDESC_FIELDS) {
        fail(err, errlen, "too many fields", 0);
        return lineno;
      }
      candesc_field_t *f = &cur->field[cur->nfields];
      snprintf(f->name, sizeof(f->name), "%s", tok[0]);
      if(parse_type(tok[1], &f->type, &f->len)) {
        fail(err, errlen, "type: bit, uN, sN, f32", tok[1]);
        return lineno;
      }
      char *end;
      long pos = strtol(tok[2], &end, 10);
      if(*end || pos < 0 || pos + f->len > cur->dlc * 8) {
        fail(err, errlen, "bit position outside the frame", tok[2]);
        return lineno;
      }
      if(f->type == CD_F32 && pos % 8) {
        fail(err, errlen, "f32 must start on a byte", tok[2]);
        return lineno;
      }
      f->pos   = (uint8_t)pos;
      f->scale = 1.0f;
      for(int i = 3; i < nt; i++) {
        if(!strncmp(tok[i], "scale=", 6)) {
          f->scale = (float)atof(tok[i] + 6);
        } else if(!strncmp(tok[i], "offset=", 7)) {
          f->offset = (float)atof(tok[i] + 7);
        } else {
          fail(err, errlen, "unknown option", tok[i]);
          return lineno;
        }
      }
      // fields of a frame must not overlap
      for(int j = 0; j < cur->nfields; j++) {
        const candesc_field_t *g = &cur->field[j];
        if(f->pos < g->pos + g->len && g->pos < f->pos + f->len) {
          fail(err, errlen, "overlaps field", g->name);
          return lineno;
        }
      }
      cur->nfields++;
    } else {
      fail(err, errlen, "cannot read", tok[0]);
      return lineno;
    }
  }
  if(!d->name[0]) {
    fail(err, errlen, "no device line", 0);
    return lineno ? lineno : 1;
  }
  for(int i = 0; i < d->nframes; i++) {
    if(parse_id(idtext[i], d->node, &d->frame[i].id)) {
      fail(err, errlen, "frame ID: 0..0x7FF, or 0xNNN+node", idtext[i]);
      return lineno;
    }
  }
  if(d->stmbl && d->nframes) {
    fail(err, errlen, "profile stmbl1 has its own frames", 0);
    return lineno;
  }
  return 0;
}

int candesc_parse_all(const char *text, candesc_t *out, int max, int *count, char *err, int errlen) {
  static char part[8192];
  const char *start = text;
  int line0         = 1;
  *count            = 0;
  while(*start) {
    // one device: up to the next line that starts with "device"
    const char *next = start;
    int lines        = 0;
    do {
      const char *nl = strchr(next, '\n');
      next           = nl ? nl + 1 : next + strlen(next);
      lines++;
    } while(*next && strncmp(next, "device", 6));
    size_t n = (size_t)(next - start);
    if(n >= sizeof(part)) {
      fail(err, errlen, "description too long", 0);
      return line0;
    }
    memcpy(part, start, n);
    part[n] = 0;
    // only comments and blank lines before the first device: skip
    int blank = 1;
    for(const char *q = part; *q && blank; q++) {
      if(*q == '#') {
        q = strchr(q, '\n');
        if(!q) {
          break;
        }
      } else if(*q != ' ' && *q != '\t' && *q != '\r' && *q != '\n') {
        blank = 0;
      }
    }
    if(!blank) {
      if(*count >= max) {
        fail(err, errlen, "too many devices", 0);
        return line0;
      }
      int l = candesc_parse(&out[*count], part, err, errlen);
      if(l) {
        return line0 + l - 1;
      }
      (*count)++;
    }
    line0 += lines;
    start = next;
  }
  return 0;
}

static uint32_t get_bits(const uint8_t *d, int pos, int len) {
  uint64_t v = 0;
  for(int i = 0; i < 8; i++) {
    v |= (uint64_t)d[i] << (8 * i);
  }
  return (uint32_t)((v >> pos) & (len == 32 ? 0xFFFFFFFFull : ((1ull << len) - 1)));
}

static void put_bits(uint8_t *d, int pos, int len, uint32_t raw) {
  uint64_t v = 0;
  for(int i = 0; i < 8; i++) {
    v |= (uint64_t)d[i] << (8 * i);
  }
  uint64_t mask = (len == 32 ? 0xFFFFFFFFull : ((1ull << len) - 1)) << pos;
  v             = (v & ~mask) | (((uint64_t)raw << pos) & mask);
  for(int i = 0; i < 8; i++) {
    d[i] = (uint8_t)(v >> (8 * i));
  }
}

void candesc_decode(const candesc_frame_t *f, const can_frame_t *cf, float *values) {
  for(int i = 0; i < f->nfields; i++) {
    const candesc_field_t *g = &f->field[i];
    uint32_t raw             = get_bits(cf->d, g->pos, g->len);
    float v;
    if(g->type == CD_F32) {
      memcpy(&v, &raw, 4);
    } else if(g->type == CD_SINT) {
      int32_t s = (int32_t)(raw << (32 - g->len)) >> (32 - g->len);  // sign-extend
      v         = (float)s;
    } else {
      v = (float)raw;
    }
    values[i] = g->type == CD_BIT ? v : v * g->scale + g->offset;
  }
}

void candesc_encode(const candesc_frame_t *f, const float *values, can_frame_t *cf) {
  memset(cf->d, 0, 8);
  cf->id  = f->id;
  cf->dlc = f->dlc;
  for(int i = 0; i < f->nfields; i++) {
    const candesc_field_t *g = &f->field[i];
    uint32_t raw;
    if(g->type == CD_F32) {
      float v = values[i] * g->scale + g->offset;
      memcpy(&raw, &v, 4);
    } else if(g->type == CD_BIT) {
      raw = values[i] != 0.0f;
    } else {
      float x = g->scale != 0.0f ? (values[i] - g->offset) / g->scale : 0.0f;
      double lo = g->type == CD_SINT ? -ldexp(1.0, g->len - 1) : 0.0;
      double hi = g->type == CD_SINT ? ldexp(1.0, g->len - 1) - 1.0 : ldexp(1.0, g->len) - 1.0;
      double r  = floor(x + 0.5);
      r         = r < lo ? lo : r > hi ? hi : r;
      raw       = (uint32_t)(int64_t)r;
    }
    put_bits(cf->d, g->pos, g->len, raw);
  }
}

int candesc_stmbl_text(char *buf, int len, float vel_scale) {
  return canp_desc_text(buf, len, (int)(vel_scale * 1000.0f + 0.5f));
}
