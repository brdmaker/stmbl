/*
 * linuxcnc/stmblcan without hardware: the MCP2515 driver and one bus's
 * period exchange against simulated MCP2515s and drives on simulated CAN
 * buses (mcp2515_sim.c). The Pi's setup: 3 buses at 1 Mbit/s, 16 MHz
 * crystals, 2 drives per bus, a 1 kHz servo thread. Prints TAP.
 */
#include "stmblcan_bus.h"
#include "mcp2515_sim.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int n, fails;
static void ok(int c, const char *name, double got) {
  n++;
  fails += !c;
  printf("%s %d - stmblcan: %s\n", c ? "ok" : "not ok", n, name);
  if(!c) {
    printf("#   got %g\n", got);
  }
}

// ---- a drive on the bus: applies the newest command at SYNC, answers it ----
typedef struct {
  sim_node_t sn;
  int node;
  float vel_scale;
  canp_cmd_t cmd, applied;
  int have_cmd;
  sscon_t con;
  uint16_t host_con;  // the host's last console field
  float slots[CANP_SLOTS];
  uint32_t syncs;
} drive_t;

static void drive_rx(sim_node_t *s, const can_frame_t *f) {
  drive_t *d = s->ctx;
  if(f->id == CANP_CMD_ID(d->node)) {
    canp_cmd_decode(f, &d->cmd, d->vel_scale);
    d->have_cmd = 1;
  } else if(f->id == CANP_SYNC_ID && f->dlc == 1) {
    d->syncs++;
    if(d->have_cmd) {
      d->applied = d->cmd;
    }
    // the console: the host's field in, ours out; answer whole lines
    uint16_t out = sscon_frame(&d->con, d->cmd.con);
    char line[64];
    int len = sscon_getline(&d->con, line, sizeof(line));
    if(len > 0) {
      char ans[80];
      int k = snprintf(ans, sizeof(ans), "node %d: %s\n", d->node, line);
      sscon_write(&d->con, ans, (uint32_t)k);
    }
    canp_fb_t fb = {.pos = d->applied.pos, .vel = d->applied.vel, .fault = 0, .index = d->applied.index_enable,
                    .in = d->applied.out, .con = out};
    can_frame_t fr;
    canp_fb_encode(&fr, d->node, &fb, d->vel_scale, f->d[0], d->slots);
    sim_node_send(&d->sn, &fr);
  }
}

static void drive_init(drive_t *d, sim_bus_t *bus, int node) {
  memset(d, 0, sizeof(*d));
  d->node      = node;
  d->vel_scale = 100.0f;
  d->sn.rx     = drive_rx;
  d->sn.ctx    = d;
  d->slots[CANP_SLOT_CURRENT] = 1.5f + 0.1f * node;
  d->slots[CANP_SLOT_POWER]   = 120.0f;
  d->slots[CANP_SLOT_TEMP]    = 41.0f;
  d->slots[CANP_SLOT_VBUS]    = 48.0f;
  sscon_reset(&d->con);
  bus->node[bus->nnode++] = &d->sn;
}

// a device that is not ours: floods the bus with flood_id frames
static uint16_t flood_id = 0x300;
typedef struct {
  sim_node_t sn;
  int per_sync;
} flood_t;
static void flood_rx(sim_node_t *s, const can_frame_t *f) {
  flood_t *fl = s->ctx;
  if(f->id == CANP_SYNC_ID) {
    for(int i = 0; i < fl->per_sync; i++) {
      can_frame_t fr = {.id = flood_id, .dlc = 8};
      sim_node_send(&fl->sn, &fr);
    }
  }
}

// ---- the Pi's side ----
static void bus_setup(stmblcan_bus_t *b, sim_chip_t *chip, int n0, int n1) {
  memset(b, 0, sizeof(*b));
  b->mcp.xfer = sim_xfer;
  b->mcp.ctx  = chip;
  b->nnodes   = 2;
  b->node[0]  = (stmblcan_node_t){.node = n0, .vel_scale = 100.0f, .con_internal = 1};
  b->node[1]  = (stmblcan_node_t){.node = n1, .vel_scale = 100.0f, .con_internal = 1};
}

int main(void) {
  // 1. bit timing
  uint8_t c[3];
  int t1 = mcp2515_timing(16000000, 1000000, c);
  ok(t1 == 0 && c[0] == 0x00 && c[1] == 0x91 && c[2] == 0x01,  // PRSEG 2, PS1 3, PS2 2
     "16 MHz, 1 Mbit/s: 8 quanta of 125 ns, sample point 75 % (CNF1 00 CNF2 91 CNF3 01)", c[1]);
  ok(mcp2515_timing(8000000, 1000000, c) < 0 && mcp2515_timing(8000000, 500000, c) == 0,
     "8 MHz crystals: 1 Mbit/s impossible, 500 kbit/s fine", 0);

  // 2. three buses, two drives each, as the Pi's overlay (spi0, 3 chip selects)
  static sim_bus_t sbus[3];
  static sim_chip_t chip[3];
  static drive_t drv[3][2];
  static stmblcan_bus_t bus[3];
  memset(sbus, 0, sizeof(sbus));
  int inits = 0;
  for(int i = 0; i < 3; i++) {
    sim_chip_init(&chip[i], &sbus[i]);
    drive_init(&drv[i][0], &sbus[i], 1 + 2 * i);
    drive_init(&drv[i][1], &sbus[i], 2 + 2 * i);
    bus_setup(&bus[i], &chip[i], 1 + 2 * i, 2 + 2 * i);
    inits += stmblcan_bus_init(&bus[i], 16000000, 1000000) == 0;
  }
  ok(inits == 3, "three MCP2515s initialised (reset, timing read back, normal mode)", inits);

  // 1000 periods of 1 ms: a moving command, the feedback a period later
  double max_err = 0, max_busy = 0, vel_err = 0;
  uint32_t spi_bytes0 = bus[0].mcp.bytes, spi_calls0 = bus[0].mcp.calls;
  float prev_cmd[3][2] = {{0}}, prev_vel[3][2] = {{0}};
  int lag_ok = 1;
  for(int k = 0; k < 1000; k++) {
    for(int i = 0; i < 3; i++) {
      for(int j = 0; j < 2; j++) {
        stmblcan_node_t *nd = &bus[i].node[j];
        if(k > 2) {  // the feedback now answers the command sent one period ago
          double e = fabs(nd->fb.pos - prev_cmd[i][j]);
          max_err  = e > max_err ? e : max_err;
          lag_ok &= e == 0.0;
          // (in slot periods vel_fb keeps the value before)
          if(!canp_slot_period((uint8_t)(bus[i].counter - 1))) {  // the SYNC the feedback answered
            vel_err = fmax(vel_err, fabs(nd->fb.vel - prev_vel[i][j]));
          }
        }
        prev_cmd[i][j]    = nd->cmd.pos;
        prev_vel[i][j]    = nd->cmd.vel;
        nd->cmd.pos       = (float)(10.0 * sin(k * 0.01 + i + j));
        nd->cmd.vel       = (float)(10.0 * 10.0 * cos(k * 0.01 + i + j) * 1.0);
        nd->cmd.enable    = 1;
        nd->cmd.out       = (uint8_t)((k >> 4) & 15);
      }
      stmblcan_bus_cycle(&bus[i]);
      double before = sbus[i].busy_us;
      sim_bus_run(&sbus[i], 1000.0);
      max_busy = fmax(max_busy, sbus[i].busy_us - before);
    }
  }
  uint32_t missed = 0, late = 0, foreign = 0, present = 0;
  for(int i = 0; i < 3; i++) {
    late += bus[i].mcp.tx_late;
    foreign += bus[i].foreign;
    for(int j = 0; j < 2; j++) {
      missed += bus[i].node[j].missed;
      present += bus[i].node[j].present;
    }
  }
  ok(present == 6 && missed == 0 && late == 0 && foreign == 0,
     "6 drives on 3 buses, 1000 periods: every feedback received, nothing late or foreign", missed + late);
  ok(lag_ok && max_err == 0.0, "feedback position = the command of one period before, exactly (float32)", max_err);
  ok(vel_err < 100.0 / 32767.0, "velocity through 16 bits: within one step of the 100 rad/s scale", vel_err);
  ok(max_busy < 700.0, "bus time per period: SYNC + 2 commands + 2 feedbacks < 70 % of 1 ms", max_busy);
  printf("# bus time per period %.0f us (worst-case stuffing)\n", max_busy);
  double bytes_per = (bus[0].mcp.bytes - spi_bytes0) / 1000.0, calls_per = (bus[0].mcp.calls - spi_calls0) / 1000.0;
  ok(calls_per <= 3.01 && bytes_per < 80, "SPI per bus and period: 3 calls (status, receive, send), < 80 bytes",
     calls_per);
  printf("# SPI per bus and period: %.1f calls, %.1f bytes: %.0f us at 5 MHz, %.0f us at 10 MHz (3 buses, "
         "transfer time only)\n",
         calls_per, bytes_per, 3 * bytes_per * 8 / 5.0, 3 * bytes_per * 8 / 10.0);
  ok(fabs(bus[1].node[0].slots[CANP_SLOT_CURRENT] - 1.8f) < 0.01 &&
         fabs(bus[1].node[0].slots[CANP_SLOT_TEMP] - 41.0f) < 0.01 &&
         fabs(bus[1].node[0].slots[CANP_SLOT_VBUS] - 48.0f) < 0.05,
     "slots: current, power, temperature, supply every 32 periods", bus[1].node[0].slots[0]);
  ok(drv[2][1].applied.out == ((999 >> 4) & 15) && drv[2][1].applied.enable == 1, "flags and outputs reach the drive",
     drv[2][1].applied.out);

  // 3. the console through the period frames
  sscon_write(&bus[1].node[1].con, "about\n", 6);
  char line[96];
  int got = -1, periods = 0;
  while(got <= 0 && periods < 200) {
    stmblcan_bus_cycle(&bus[1]);
    sim_bus_run(&sbus[1], 1000.0);
    got = sscon_getline(&bus[1].node[1].con, line, sizeof(line));
    periods++;
  }
  ok(got > 0 && !strcmp(line, "node 4: about"), "console: a line to drive 4 and its answer back", periods);
  printf("# console round trip: %d periods for 6 + 14 bytes\n", periods);

  // 4. a drive missing: its feedback stops, the other one still ACKs the bus
  sbus[0].nnode = 1;  // drive 2 unplugged
  for(int k = 0; k < 20; k++) {
    stmblcan_bus_cycle(&bus[0]);
    sim_bus_run(&sbus[0], 1000.0);
  }
  ok(!bus[0].node[1].present && bus[0].node[0].present && bus[0].mcp.tx_late == 0,
     "a drive unplugged: not present after the timeout, the other one unaffected", bus[0].node[1].present);

  // 5. nobody on the bus: no ACK, frames stay pending and are aborted a period later
  sbus[0].nnode = 0;
  uint32_t late0 = bus[0].mcp.tx_late;
  for(int k = 0; k < 20; k++) {
    stmblcan_bus_cycle(&bus[0]);
    sim_bus_run(&sbus[0], 1000.0);
  }
  ok(bus[0].mcp.tx_late - late0 >= 19 * 3 - 3 && !bus[0].node[0].present,
     "an empty bus: unacknowledged frames aborted each period (tx_late), drives not present",
     bus[0].mcp.tx_late - late0);

  // 6. a foreign device flooding, with identifiers of higher and of lower
  // priority than the feedback: the acceptance filters keep it out of the
  // receive buffers either way
  static flood_t fl;
  fl.sn.rx    = flood_rx;
  fl.sn.ctx   = &fl;
  fl.per_sync = 2;  // 2 x 135 us: with the period's own 605 us still under 1 ms
  sbus[2].node[sbus[2].nnode++] = &fl.sn;
  for(int pass = 0; pass < 2; pass++) {
    flood_id = pass ? 0x050 : 0x300;  // 0x050 wins arbitration over the feedback (0x185, 0x186)
    uint32_t missed2 = bus[2].node[0].missed + bus[2].node[1].missed, ovf = bus[2].mcp.rx_overflow;
    for(int k = 0; k < 512; k++) {
      stmblcan_bus_cycle(&bus[2]);
      sim_bus_run(&sbus[2], 1000.0);
    }
    uint32_t lost = bus[2].node[0].missed + bus[2].node[1].missed - missed2;
    ok(lost == 0 && bus[2].mcp.rx_overflow == ovf && bus[2].node[0].present && bus[2].node[1].present,
       pass ? "a foreign device, 2 frames a period at higher priority (0x050): filtered out, no feedback lost"
            : "a foreign device, 2 frames a period at lower priority (0x300): filtered out, no feedback lost",
       lost);
  }
  // 3 high-priority frames a period: the bus itself is overloaded (> 1 ms)
  fl.per_sync = 3;
  uint32_t m3 = bus[2].node[0].missed + bus[2].node[1].missed, ovf3 = bus[2].mcp.rx_overflow;
  for(int k = 0; k < 512; k++) {
    stmblcan_bus_cycle(&bus[2]);
    sim_bus_run(&sbus[2], 1000.0);
  }
  ok(bus[2].node[0].missed + bus[2].node[1].missed > m3 && bus[2].mcp.rx_overflow == ovf3,
     "  ...3 frames a period: the bus is full (> 1 ms), feedback misses periods; still no buffer overflow",
     bus[2].node[0].missed + bus[2].node[1].missed - m3);
  // without the filters the higher-priority flood takes the buffers
  static sim_bus_t sb5;
  static sim_chip_t ch5;
  static drive_t d5[2];
  static flood_t fl5;
  memset(&sb5, 0, sizeof(sb5));
  sim_chip_init(&ch5, &sb5);
  drive_init(&d5[0], &sb5, 5);
  drive_init(&d5[1], &sb5, 6);
  fl5 = (flood_t){.sn = {.rx = flood_rx}, .per_sync = 2};
  fl5.sn.ctx = &fl5;
  sb5.node[sb5.nnode++] = &fl5.sn;
  stmblcan_bus_t b5;
  bus_setup(&b5, &ch5, 5, 6);
  stmblcan_bus_init(&b5, 16000000, 1000000);
  mcp2515_init(&b5.mcp, 16000000, 1000000, 0, 0);  // again, filters off
  flood_id = 0x050;
  for(int k = 0; k < 512; k++) {
    stmblcan_bus_cycle(&b5);
    sim_bus_run(&sb5, 1000.0);
  }
  ok(b5.node[0].fb_frames + b5.node[1].fb_frames < 10 && !b5.node[0].present && b5.mcp.rx_overflow > 0,
     "  ...without the filters, the same flood takes both buffers: the feedback never gets through",
     b5.node[0].fb_frames + b5.node[1].fb_frames);

  // 7. no chip on a chip select
  static sim_bus_t sb4;
  static sim_chip_t none;
  sim_chip_init(&none, &sb4);
  none.present = 0;
  stmblcan_bus_t b4;
  bus_setup(&b4, &none, 1, 2);
  ok(stmblcan_bus_init(&b4, 16000000, 1000000) == -3, "no MCP2515 on a chip select: init fails (CANSTAT)", 0);

  printf("1..%d\n# %d/%d passed\n", n, n - fails, n);
  return fails ? 1 : 0;
}
