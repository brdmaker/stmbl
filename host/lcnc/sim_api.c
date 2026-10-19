/*
 * The simulated drive behind the stmbl_sim LinuxCNC module. See
 * stmbl_sim_api.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "can_host.h"
#include "drive.h"
#include "pty.h"
#include "sim_io.h"
#include "stmbl_sim_api.h"

static long carry_ns = 0;
static int can_on    = 0;

void stmbl_sim_can(void) {
  can_host_attach();
  can_on = 1;
}

int stmbl_sim_can_xfer(void *ctx, void *segs, int n) {
  (void)ctx;
  return can_host_xfer((mcp_seg_t *)segs, n);
}

// one rt cycle, with the bus during it (as host/main.c's run_cycles)
static void tick(void) {
  if(can_on) {
    can_host_run(RT_PERIOD * 1e6);
  }
  drive_tick();
}

void stmbl_sim_can_wait(void *ctx, int us) {
  (void)ctx;
  for(int t = 0; t < us; t += (int)(RT_PERIOD * 1e6)) {
    tick();
  }
}

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
  long rt_ns           = (long)(RT_PERIOD * 1e9 + 0.5);

  io->pos_cmd      = pd->pos_cmd;
  io->vel_cmd      = pd->vel_cmd;
  io->scale        = pd->scale;
  io->enable       = pd->enable;
  io->index_enable = pd->index_enable;
  io->con_cmd      = pd->con_cmd;
  memcpy(io->out, pd->out, sizeof(io->out));
  io->seq++;

  // The real drive answers a packet as soon as it has it (hm2 reads that
  // answer at the start of the next servo period), so run the rt cycle in
  // which sserial0 takes the packet and replies before handing back the
  // feedback. Returning the previous packet's answer added a full period
  // of feedback delay (0.5 mm of following error at 500 mm/s).
  carry_ns += period_ns;
  if(carry_ns >= rt_ns) {
    carry_ns -= rt_ns;
    tick();
  }

  pd->pos_fb          = io->pos_fb;
  pd->vel_fb          = io->vel_fb;
  pd->current         = io->current;
  pd->power           = io->power;
  pd->con_fb          = io->con_fb;
  pd->fault           = io->fault;
  pd->index_enable_fb = io->index_enable_fb;
  memcpy(pd->in, io->in, sizeof(pd->in));

  while(carry_ns >= rt_ns) {
    carry_ns -= rt_ns;
    tick();
  }
}
