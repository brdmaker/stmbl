/*
 * lathe_logic: a lathe's turret and spindle, without HAL (the lathe module's glue is lathe.c; the host's
 * tests are host/tests/test_lathe.c).
 *
 * The turret: a stepper (step/dir, a stepgen in position mode) turns it; one index pulse a turret turn
 * gives its angle; it locks running backwards onto its pawl (an Emco Compact 5's). A change:
 *   - not homed yet: forward until the index, which sets station 1's angle (offset after the index);
 *   - forward to the station's angle (never backwards: that is the lock), past it by overtravel;
 *   - back by overtravel + lock at the lock speed: onto the pawl, short of the stall;
 *   - a dwell, then done (held until the request drops).
 * Positions are turret turns. An index seen elsewhere than where it should be (the turret slipped, a
 * lost step) is a fault, as are no index within 1.5 turns while homing and a change that takes too long.
 *
 * The spindle: an stmbl drive in velocity mode. Its position feedback (motor radians, wrapped or not)
 * is summed into spindle turns (spindle.0.revs) through the motor:spindle ratio; LinuxCNC's
 * index-enable (G33, G76) is cleared at the spindle's index: a sensor (one pulse a spindle turn) or,
 * without one, the motor's zero angle (right only when the ratio is a whole number of motor turns).
 */
#ifndef LATHE_LOGIC_H
#define LATHE_LOGIC_H

enum { TUR_IDLE, TUR_HOME, TUR_FWD, TUR_LOCK, TUR_DWELL, TUR_DONE, TUR_FAULT };
enum { TUR_OK, TUR_E_NOINDEX, TUR_E_SLIP, TUR_E_TIMEOUT, TUR_E_STATION };

typedef struct {
  /* set up */
  int stations;          /* 1..32 */
  double offset;         /* station 1 after the index [turns] */
  double overtravel;     /* past the station before locking [turns] */
  double lock;           /* back from the station onto the pawl [turns] */
  double speed, lock_speed, acc;  /* [turns/s], [turns/s^2] */
  double dwell, timeout; /* [s] */
  double slip;           /* an index this far from where it should be: a fault [turns] */
  /* state */
  int state, err, homed, station, target, last_index;
  double cmd, vel, goal, t, t_all, index_pos;
} turret_t;

void turret_init(turret_t *t);
/* one period: request (a change wanted, held until done), tool (the station: 1..stations; 0 none),
 * index (the sensor), fb (the stepgen's position [turns]); returns 1 when done */
int turret_step(turret_t *t, double dt, int request, int tool, int index, double fb);
/* a tool number's station: 1..stations (T7 on 6 stations: station 1 again, its own offsets) */
int turret_station(const turret_t *t, int tool);
const char *turret_error(int err);

typedef struct {
  double ratio;          /* motor turns a spindle turn */
  int use_sensor;        /* the index from a sensor (else the motor's zero angle) */
  /* state */
  int init, last_sensor;
  double last_fb, acc, origin, rev0, cmd_angle;  /* acc: turned since the start [motor rad]; origin: the angle then */
} lspindle_t;

void lspindle_init(lspindle_t *s, double ratio);
/* speed [spindle turns/s] -> the drive's velocity command [motor rad/s] (and *pos_cmd, wrapped to ±pi);
 * fb [motor rad] -> spindle turns since the last index; *index_enable cleared at the index */
double lspindle_cmd(lspindle_t *s, double dt, double speed, double *pos_cmd);
double lspindle_fb(lspindle_t *s, double fb, int sensor, int *index_enable);

#endif
