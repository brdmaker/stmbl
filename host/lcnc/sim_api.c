/*
 * The simulated drive behind the stmbl_sim LinuxCNC module. See
 * stmbl_sim_api.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drive.h"
#include "pty.h"
#include "sim_io.h"
#include "stmbl_sim_api.h"

static long carry_ns = 0;

int stmbl_sim_open(const char *conf, const char *sim, const char *pty) {
  char *text = sim_read_file(conf);
  if(!text) {
    return -1;
  }
  drive_set_flash(text);
  free(text);
  if(sim && sim[0]) {
    char *extra = sim_read_file(sim);
    if(!extra) {
      return -2;
    }
    drive_set_sim_extra(extra);
    free(extra);
  }
  if(sim_io_start(pty && pty[0] ? pty : NULL, conf) < 0) {
    return -3;
  }
  drive_set_nrt(1);
  drive_boot();
  return 0;
}

void stmbl_sim_close(void) {
  sim_io_stop();
}

void stmbl_sim_update(stmbl_sim_pd_t *pd, long period_ns) {
  sim_sserial_io_t *io = &sim_sserial_io;

  pd->pos_fb          = io->pos_fb;
  pd->vel_fb          = io->vel_fb;
  pd->current         = io->current;
  pd->fault           = io->fault;
  pd->index_enable_fb = io->index_enable_fb;
  memcpy(pd->in, io->in, sizeof(pd->in));

  io->pos_cmd      = pd->pos_cmd;
  io->vel_cmd      = pd->vel_cmd;
  io->scale        = pd->scale;
  io->enable       = pd->enable;
  io->index_enable = pd->index_enable;
  memcpy(io->out, pd->out, sizeof(io->out));
  io->seq++;

  carry_ns += period_ns;
  long rt_ns = (long)(RT_PERIOD * 1e9 + 0.5);
  while(carry_ns >= rt_ns) {
    carry_ns -= rt_ns;
    drive_tick();
  }
}
