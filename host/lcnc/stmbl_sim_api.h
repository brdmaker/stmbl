/*
 * Interface between the LinuxCNC module (stmbl_sim.c, built against
 * LinuxCNC's hal.h) and the simulated drive (sim_api.c, built against
 * stmbl's hal.h). The two HAL headers clash, so this header includes
 * neither, and the Makefile hides every other symbol of the drive.
 */
#pragma once
#include <stdint.h>

typedef struct {
  // LinuxCNC -> drive, as in the smart-serial process data
  float pos_cmd;
  float vel_cmd;
  float scale;
  uint8_t out[4];
  uint8_t enable;
  uint8_t index_enable;
  // drive -> LinuxCNC
  float pos_fb;
  float vel_fb;
  float current;
  float power;  // the drive's input power [W]
  uint8_t in[4];
  uint8_t fault;
  uint8_t index_enable_fb;
} stmbl_sim_pd_t;

// Boot the drive from conf (also its flash), plus optional sim-only
// settings; console on a pty symlinked at pty (NULL: none). 0 on success.
int stmbl_sim_open(const char *conf, const char *sim, const char *pty);
void stmbl_sim_close(void);

// One LinuxCNC servo period: exchange process data, then run the drive for
// period_ns (in 200 us rt cycles; the remainder carries over).
void stmbl_sim_update(stmbl_sim_pd_t *pd, long period_ns);

// --- for hosts that run several drives (host/arm: one copy per dlmopen
// namespace) and couple them to their own mechanics ---------------------

// Boot from config text (and optional sim-only text) without the console
// pty and its thread; console output goes to stdout. 0 on success.
int stmbl_sim_boot(const char *conf_text, const char *sim_text);
// One 200 us rt cycle (frt cycles included), no process data exchange.
void stmbl_sim_tick(void);
// The value of pin "comp<inst>.pin" (e.g. "hv0.sim_load"), or NULL. Reads
// follow links; write only unlinked pins.
float *stmbl_sim_pin(const char *name);
// Run one console line (output to stdout).
void stmbl_sim_cmd(const char *line);
double stmbl_sim_time(void);

