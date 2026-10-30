/*
 * vis3d: small 4x4 matrix math (column-major, as GL takes it).
 */
#ifndef VMATH_H
#define VMATH_H

#include <math.h>
#include <string.h>

typedef struct {
  float m[16];  // column-major: m[col * 4 + row]
} mat4;

static inline mat4 m4_identity(void) {
  mat4 r;
  memset(&r, 0, sizeof(r));
  r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
  return r;
}

static inline mat4 m4_mul(mat4 a, mat4 b) {
  mat4 r;
  for(int c = 0; c < 4; c++) {
    for(int rr = 0; rr < 4; rr++) {
      float s = 0.0f;
      for(int k = 0; k < 4; k++) {
        s += a.m[k * 4 + rr] * b.m[c * 4 + k];
      }
      r.m[c * 4 + rr] = s;
    }
  }
  return r;
}

static inline mat4 m4_translate(float x, float y, float z) {
  mat4 r   = m4_identity();
  r.m[12] = x, r.m[13] = y, r.m[14] = z;
  return r;
}

static inline mat4 m4_scale(float x, float y, float z) {
  mat4 r  = m4_identity();
  r.m[0] = x, r.m[5] = y, r.m[10] = z;
  return r;
}

static inline mat4 m4_rot_x(float a) {
  mat4 r = m4_identity();
  float c = cosf(a), s = sinf(a);
  r.m[5] = c, r.m[6] = s, r.m[9] = -s, r.m[10] = c;
  return r;
}

static inline mat4 m4_rot_y(float a) {
  mat4 r = m4_identity();
  float c = cosf(a), s = sinf(a);
  r.m[0] = c, r.m[2] = -s, r.m[8] = s, r.m[10] = c;
  return r;
}

static inline mat4 m4_rot_z(float a) {
  mat4 r = m4_identity();
  float c = cosf(a), s = sinf(a);
  r.m[0] = c, r.m[1] = s, r.m[4] = -s, r.m[5] = c;
  return r;
}

// R = Rz(c) Ry(b) Rx(a), as genserkins' A B C
static inline mat4 m4_rpy(float a, float b, float c) {
  return m4_mul(m4_rot_z(c), m4_mul(m4_rot_y(b), m4_rot_x(a)));
}

// Craig (modified) DH: RotX(alpha) TransX(a) RotZ(theta) TransZ(d)
static inline mat4 m4_dh(float a, float alpha, float d, float theta) {
  return m4_mul(m4_mul(m4_rot_x(alpha), m4_translate(a, 0, 0)), m4_mul(m4_rot_z(theta), m4_translate(0, 0, d)));
}

static inline mat4 m4_perspective(float fovy, float aspect, float zn, float zf) {
  mat4 r;
  memset(&r, 0, sizeof(r));
  float f  = 1.0f / tanf(fovy / 2.0f);
  r.m[0]  = f / aspect;
  r.m[5]  = f;
  r.m[10] = (zf + zn) / (zn - zf);
  r.m[11] = -1.0f;
  r.m[14] = 2.0f * zf * zn / (zn - zf);
  return r;
}

static inline void v3_sub(const float *a, const float *b, float *o) {
  o[0] = a[0] - b[0], o[1] = a[1] - b[1], o[2] = a[2] - b[2];
}
static inline void v3_cross(const float *a, const float *b, float *o) {
  float x = a[1] * b[2] - a[2] * b[1], y = a[2] * b[0] - a[0] * b[2], z = a[0] * b[1] - a[1] * b[0];
  o[0] = x, o[1] = y, o[2] = z;
}
static inline float v3_dot(const float *a, const float *b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
static inline void v3_norm(float *a) {
  float l = sqrtf(v3_dot(a, a));
  if(l > 1e-12f) {
    a[0] /= l, a[1] /= l, a[2] /= l;
  }
}

// a view matrix: eye looking at center, up = +Z (the robot's base frame)
static inline mat4 m4_look_at(const float *eye, const float *center) {
  float f[3], s[3], u[3], up[3] = {0, 0, 1};
  v3_sub(center, eye, f);
  v3_norm(f);
  v3_cross(f, up, s);
  v3_norm(s);
  v3_cross(s, f, u);
  mat4 r  = m4_identity();
  r.m[0]  = s[0], r.m[4] = s[1], r.m[8] = s[2];
  r.m[1]  = u[0], r.m[5] = u[1], r.m[9] = u[2];
  r.m[2]  = -f[0], r.m[6] = -f[1], r.m[10] = -f[2];
  r.m[12] = -v3_dot(s, eye);
  r.m[13] = -v3_dot(u, eye);
  r.m[14] = v3_dot(f, eye);
  return r;
}

// p' = M p (w = 1)
static inline void m4_point(mat4 a, const float *p, float *o) {
  float x = a.m[0] * p[0] + a.m[4] * p[1] + a.m[8] * p[2] + a.m[12];
  float y = a.m[1] * p[0] + a.m[5] * p[1] + a.m[9] * p[2] + a.m[13];
  float z = a.m[2] * p[0] + a.m[6] * p[1] + a.m[10] * p[2] + a.m[14];
  o[0] = x, o[1] = y, o[2] = z;
}
static inline void m4_dir(mat4 a, const float *p, float *o) {
  float x = a.m[0] * p[0] + a.m[4] * p[1] + a.m[8] * p[2];
  float y = a.m[1] * p[0] + a.m[5] * p[1] + a.m[9] * p[2];
  float z = a.m[2] * p[0] + a.m[6] * p[1] + a.m[10] * p[2];
  o[0] = x, o[1] = y, o[2] = z;
}

#endif
