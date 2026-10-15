/*
 * Power quotas of a machine's supply: fuses (or breakers) feeding several
 * stmbl drives, watched from the drives' input power (sserial "power",
 * pe0.p_in), and the feed scale that keeps every fuse from blowing.
 *
 * A quota is one fuse: its rated current In, the voltage its load sees
 * (phase-neutral for one phase), the load's power factor (a rectifier
 * with a bus capacitor draws peaky current: 0.6-0.9), its trip profile and
 * the share of each drive's power that flows through it: 1 for a
 * single-phase drive on that phase, 1/3 per phase for a three-phase
 * drive, any mix (several drives on one fuse, two per phase, ...). Its
 * current is I = sum(share * p) / (V * pf), the load m = I / In.
 *
 * Trip profiles: a thermal curve, at m x In it trips after t seconds
 * (log-log between the points, never below the first m), a magnetic
 * (instant) trip at mag x In (0: none), and a cooling time constant. The
 * fuse's heating is followed as damage: d' = 1 / t(m) while over the
 * first point (1 = it trips), d' = -d / tau below it.
 *
 * Built-in profiles, the fast edge of the standard bands (so the model
 * trips no later than the real one): B, C, D (IEC 60898-1 breakers: the
 * same thermal curve, magnetic at 3, 5, 10 In), gG (IEC 60269 fuse).
 *
 * The feed scale: each quota asks for 1 while its damage is under
 * `start`, falling linearly to f_min at `margin`; and, with a magnetic
 * trip, for proportionally less while m is over mag_margin * mag. The
 * smallest wins; it follows falling requests fast (t_down) and rises
 * slowly (t_up). A quota whose damage reaches `trip` raises the alarm
 * (latched until reset): stop the machine before the fuse goes (e.g. a
 * load that does not fall with the feed: holding against gravity).
 *
 * Config lines (powerquota's config file):
 *   profile NAME M1 T1 M2 T2 ...   thermal curve (m x In, seconds)
 *   magnetic NAME M                instant trip at M x In (0 none)
 *   cool NAME TAU                  cooling time constant [s]
 *   quota NAME PROFILE IN VOLT [PF]  a fuse [A, V]; PF default 0.9
 *   load QUOTA DRIVE SHARE         drive DRIVE's (0-based) share through it
 *   drives N                       number of drive power inputs
 *   margin D / start D / trip D    damage levels (0.5 / 0.2 / 0.9)
 *   fmin F                         lowest feed scale (0.05)
 *   regen R                        share of regenerated (negative) power
 *                                  that offsets the others (0: separate
 *                                  rectifiers, 1: a common DC bus)
 *   magmargin K                    keep m under K * mag (0.7)
 *   rates TDOWN TUP                feed scale time constants [s] (0.02 1)
 */
#ifndef QUOTA_H
#define QUOTA_H

#define PQ_MAXQ 8
#define PQ_MAXD 16
#define PQ_MAXP 8
#define PQ_MAXPT 12

typedef struct {
  char name[16];
  int n;
  double m[PQ_MAXPT], t[PQ_MAXPT];  // thermal curve
  double mag;                       // magnetic trip [x In], 0 none
  double tau;                       // cooling [s]
} pq_profile;

typedef struct {
  char name[16];
  int prof;
  double in, volt, pf;
  double share[PQ_MAXD];
  // state / outputs
  double p;       // power through it [W]
  double i;       // current [A]
  double m;       // load, I / In
  double damage;  // 0 .. 1 (blown)
  double want;    // the feed scale it asks for
} pq_quota;

typedef struct {
  pq_profile prof[PQ_MAXP];
  int nprof;
  pq_quota q[PQ_MAXQ];
  int nq, nd;
  double margin, start, trip, f_min, regen, mag_margin, t_down, t_up;
  // outputs
  double scale;      // feed scale (motion.adaptive-feed)
  double total;      // sum of the drives' power [W]
  int limiting;      // scale < 1
  int alarm;         // a quota's damage reached `trip` (latched)
  int alarm_quota;   // which (-1 none)
} pq_system;

void pq_init(pq_system *s);  // defaults, built-in profiles B C D gG
// a config line: 0 used (or empty), -1 malformed (message in err)
int pq_line(pq_system *s, const char *line, char *err, int errlen);
int pq_file(pq_system *s, const char *path, char *err, int errlen);
// trip time [s] of profile k at load m (1e30: never)
double pq_trip_time(const pq_profile *p, double m);
// one period: the drives' power [W]
void pq_step(pq_system *s, const double *p_drive, double dt);
void pq_reset(pq_system *s);  // clears the alarm

#endif
