/*
 * The fixed-bridge mill's mechanics, see mplant.h.
 */
#include "mplant.h"

#include <math.h>
#include <string.h>

double mp_damp(double k, double m, double zeta) {
  return 2.0 * zeta * sqrt(k * m);
}

void mp_defaults(mp_plant *p) {
  memset(p, 0, sizeof(*p));
  p->g = 9.81;
  p->vs = 1e-4;
  for(int k = 0; k < 3; k++) {
    p->p[k] = 0.01, p->jm[k] = 5e-5, p->ea[k] = 2.8e7, p->l0[k] = 0.1, p->ldir[k] = 1.0;
    p->ka[k] = 1.5e8, p->zeta[k] = 0.03;
    p->smin[k] = -1.0, p->smax[k] = 1.0;
  }
  p->mf = 140, p->mt = 40, p->mb = 30, p->mc = 12, p->mz = 10, p->mh = 5;
}

double mp_kscrew(const mp_plant *p, int k) {
  double L = p->l0[k] + p->ldir[k] * p->s[k];
  L = L < 0.02 ? 0.02 : L;
  double ks = p->ea[k] / L;
  return 1.0 / (1.0 / ks + 1.0 / p->ka[k]);
}

void mp_khead(const mp_plant *p, double *kx, double *ky) {
  double span = p->smax[MZ] - p->smin[MZ];
  double u = span > 0 ? (p->smax[MZ] - p->s[MZ]) / span : 0.0;  // 0 top .. 1 bottom
  u = u < 0 ? 0 : u > 1 ? 1 : u;
  *kx = p->khx[0] + (p->khx[1] - p->khx[0]) * u;
  *ky = p->khy[0] + (p->khy[1] - p->khy[0]) * u;
}

static double fric(double c, double vis, double v, double vs) {  // the force opposing v
  return c * tanh(v / vs) + vis * v;
}

// the load each screw moves (for its damping)
static double load_mass(const mp_plant *p, int k) {
  return k == MX ? p->mc + p->mz + p->mh : k == MY ? p->mt : p->mz + p->mh;
}

void mp_step(mp_plant *p, const double *T, double dt) {
  double a_s[3], a_w[3];
  for(int k = 0; k < 3; k++) {
    double r = p->p[k] * (1.0 + p->lead[k]) / (2.0 * M_PI);  // m per rad, as the nut really moves
    double kk = mp_kscrew(p, k), c = mp_damp(kk, load_mass(p, k), p->zeta[k]);
    p->F[k] = kk * (p->phi[k] * r - p->s[k]) + c * (p->w[k] * r - p->v[k]);
    a_w[k] = (T[k] - p->F[k] * r - fric(p->tc[k], p->tv[k], p->w[k], p->vs / r)) / p->jm[k];
  }
  double kx, ky;
  mp_khead(p, &kx, &ky);
  double chx = mp_damp(kx, p->mh, p->zh), chy = mp_damp(ky, p->mh, p->zh);
  double fhx = kx * p->hx + chx * p->vhx, fhy = ky * p->hy + chy * p->vhy;  // pull on the head tip back
  double fb = p->kb * p->yb + p->cb * p->vyb;

  // x: base (with table and bridge), carriage (with Z slide), head tip
  double fgx = fric(p->fc[MX], p->fv[MX], p->v[MX], p->vs);
  double Mfx = p->mf + p->mt + p->mb;
  double afx = (-p->kfx * p->xf - p->cfx * p->vxf - p->F[MX] + fgx - p->cut[MX]) / Mfx;
  double acx = (p->F[MX] - fgx + fhx) / (p->mc + p->mz);
  double ahx = (-fhx + p->cut[MX]) / p->mh;

  // y: base, table (moves -y as machine Y grows), bridge (with carriage, Z), head tip
  double fgy = fric(p->fc[MY], p->fv[MY], p->v[MY], p->vs);
  double afy = (-p->kfy * p->yf - p->cfy * p->vyf + p->F[MY] - fgy + fb) / p->mf;
  double aty = (-p->F[MY] + fgy - p->cut[MY]) / p->mt;
  double aby = (-fb + fhy) / (p->mb + p->mc + p->mz);
  double ahy = (-fhy + p->cut[MY]) / p->mh;

  // z: the slide and head against gravity (base rigid vertically)
  double fgz = fric(p->fc[MZ], p->fv[MZ], p->v[MZ], p->vs);
  double az = (p->F[MZ] - fgz - (p->mz + p->mh) * p->g + p->cut[MZ]) / (p->mz + p->mh);

  a_s[MX] = acx - afx;
  a_s[MY] = -(aty - afy);
  a_s[MZ] = az;
  double ayb = aby - afy, ahx_r = ahx - acx, ahy_r = ahy - aby;

  // semi-implicit Euler
  for(int k = 0; k < 3; k++) {
    p->w[k] = p->brake[k] ? 0.0 : p->w[k] + a_w[k] * dt;
    p->phi[k] += p->w[k] * dt;
    p->v[k] += a_s[k] * dt, p->s[k] += p->v[k] * dt;
    p->acc[k] = a_s[k];
    if(p->s[k] < p->smin[k] || p->s[k] > p->smax[k]) {  // end stop: dead stop
      p->s[k] = p->s[k] < p->smin[k] ? p->smin[k] : p->smax[k];
      p->v[k] = 0.0;
      p->hits[k]++;
    }
  }
  p->vxf += afx * dt, p->xf += p->vxf * dt;
  p->vyf += afy * dt, p->yf += p->vyf * dt;
  p->vyb += ayb * dt, p->yb += p->vyb * dt;
  p->vhx += ahx_r * dt, p->hx += p->vhx * dt;
  p->vhy += ahy_r * dt, p->hy += p->vhy * dt;
}

void mp_tool(const mp_plant *p, double out[3]) {
  out[0] = p->s[MX] + p->hx;
  out[1] = p->s[MY] + p->yb + p->hy;
  out[2] = p->s[MZ];
}
