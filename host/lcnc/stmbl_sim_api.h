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
