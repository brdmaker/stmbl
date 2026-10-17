/*
 * linuxcnc/stmblcan without hardware: the MCP2515 driver and one bus's
 * period exchange against simulated MCP2515s and drives on simulated CAN
 * buses (mcp2515_sim.c). The Pi's setup: 3 buses at 1 Mbit/s, 16 MHz
 * crystals, 2 drives per bus, a 1 kHz servo thread. Prints TAP.
 */
#include "stmblcan_bus.h"
#include "mcp2515_sim.h"
#include "candesc.h"
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
  } else if(f->id == CANP_DESC_REQ_ID(d->node) && f->d[0] == CANP_DESC_READ) {
    char text[96];
    int len = canp_desc_text(text, sizeof(text), (int)(d->vel_scale * 1000.0f + 0.5f));
    can_frame_t fr;
    canp_desc_chunk(&fr, d->node, text, len + 1, f->d[1]);
    sim_node_send(&d->sn, &fr);
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

// a device without our firmware: an I/O module that sends its status every
// millisecond on its own clock (0x1A0) and takes outputs (0x220)
typedef struct {
  sim_node_t sn;
  uint8_t di;
  float ain0, temp;
  int ready;
  can_frame_t last_out;
  uint32_t outs;
} iomod_t;
static void iomod_rx(sim_node_t *s, const can_frame_t *f) {
  iomod_t *m = s->ctx;
  if(f->id == 0x220) {
    m->last_out = *f;
    m->outs++;
  }
}
static void iomod_tick(iomod_t *m) {  // every 1 ms
  can_frame_t f = {.id = 0x1A0, .dlc = 8};
  uint16_t a    = (uint16_t)(m->ain0 / 0.000805f + 0.5f);
  int16_t t     = (int16_t)((m->temp + 40.0f) / 0.1f + (m->temp >= -40 ? 0.5f : -0.5f));
  f.d[0]        = m->di;
  f.d[1]        = (uint8_t)a;  // ain0: u12 at bit 8
  f.d[2]        = (uint8_t)((a >> 8) & 0x0F);
  f.d[3]        = (uint8_t)t;  // temp: s16 at bit 24
  f.d[4]        = (uint8_t)((uint16_t)t >> 8);
  f.d[5]        = (uint8_t)(m->ready ? 1 : 0);  // ready: bit 40
  sim_node_send(&m->sn, &f);
}

static const char *iomod_desc =
    "# an I/O module without stmbl firmware\n"
    "device iomod\n"
    "node 32\n"
    "frame status 0x1A0 8 rx\n"
    "  di     u8   0\n"
    "  ain0   u12  8   scale=0.000805\n"
    "  temp   s16  24  scale=0.1 offset=-40\n"
    "  ready  bit  40\n"
    "frame outputs 0x220 4 tx every 2\n"
    "  do     u8   0\n"
    "  pwm    u16  8   scale=0.0015259\n";

static void wait_bus(void *ctx, int us) {
  sim_bus_run((sim_bus_t *)ctx, us);
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

  // 8. descriptions: the text format
  candesc_t d;
  char err[96];
  int pr = candesc_parse(&d, iomod_desc, err, sizeof(err));
  ok(pr == 0 && !strcmp(d.name, "iomod") && d.node == 32 && d.nframes == 2 && d.frame[0].id == 0x1A0 &&
         d.frame[0].nfields == 4 && d.frame[0].field[1].len == 12 && d.frame[0].field[2].type == CD_SINT &&
         d.frame[1].dir == CD_TX && d.frame[1].every == 2,
     "description: an I/O module's frames and fields parsed", pr);
  struct {
    const char *text, *why;
    int line;
  } bad[] = {
      {"device x\nframe a 0x100 4 rx\n  p u12 8\n  q u8 12\n", "overlaps field", 4},
      {"device x\nframe a 0x100 2 rx\n  p u12 8\n", "bit position outside the frame", 3},
      {"device x\nframe a 0x100 8 rx\n  p q7 0\n", "type: bit, uN, sN, f32", 3},
      {"frame a 0x100 8 rx\n  p u8 0\n", "no device line", 2},
      {"device x\nframe a 0x900 8 rx\n", "frame ID", 2},
      {"device x\nframe a 0x180+node 8 tx\n", "frame ID", 2},  // +node with no node line: 0x180 + 0 ok?
      {"device x\nprofile stmbl1\nframe a 0x100 8 rx\n", "profile stmbl1 has its own frames", 3},
  };
  int bad_ok = 1;
  for(unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    int l = candesc_parse(&d, bad[i].text, err, sizeof(err));
    if(i == 5) {  // "+node" without a node line resolves to node 0: accepted
      bad_ok &= l == 0 && d.frame[0].id == 0x180;
      continue;
    }
    bad_ok &= l == bad[i].line && strstr(err, bad[i].why) != 0;
    if(!(l == bad[i].line && strstr(err, bad[i].why))) {
      printf("#   case %u: line %d: %s\n", i, l, err);
    }
  }
  ok(bad_ok, "description errors: the line and the reason (overlap, outside the frame, type, no device, ID, profile)",
     0);

  // the generic codec
  candesc_parse(&d, iomod_desc, err, sizeof(err));
  float vin[4] = {0xA5, 2.5f, -12.3f, 1}, vout[4];
  can_frame_t cf;
  candesc_encode(&d.frame[0], vin, &cf);
  candesc_decode(&d.frame[0], &cf, vout);
  ok(vout[0] == 0xA5 && fabsf(vout[1] - 2.5f) < 0.000805f && fabsf(vout[2] + 12.3f) < 0.06f && vout[3] == 1,
     "codec: u8, u12 with scale, s16 with scale and offset (negative), bit: round trip", vout[2]);
  float big[4] = {300, 99.0f, 9999.0f, 0};
  candesc_encode(&d.frame[0], big, &cf);
  candesc_decode(&d.frame[0], &cf, vout);
  ok(vout[0] == 255 && fabsf(vout[1] - 4095 * 0.000805f) < 1e-4f && vout[2] > 3000.0f && vout[2] < 3280.0f,
     "codec: out-of-range values clamp to the field's range", vout[1]);
  char stext[96];
  candesc_stmbl_text(stext, sizeof(stext), 12.5f);
  pr = candesc_parse(&d, stext, err, sizeof(err));
  ok(pr == 0 && d.stmbl && !strcmp(d.name, "stbl") && fabsf(d.vel_scale - 12.5f) < 1e-6f,
     "a stmbl drive's description: profile stmbl1 with its vel_scale", d.vel_scale);

  // 9. the period budget (3 out incl. SYNC, 2 in)
  stmblcan_bus_t bb;
  candesc_t io, st;
  candesc_parse(&io, iomod_desc, err, sizeof(err));
  candesc_parse(&st, "device stbl\nprofile stmbl1\nnode 1\n", err, sizeof(err));
  memset(&bb, 0, sizeof(bb));
  stmblcan_bus_add(&bb, &st);
  stmblcan_bus_add(&bb, &io);
  int c1 = stmblcan_bus_check(&bb, err, sizeof(err));
  st.node = 2;
  stmblcan_bus_add(&bb, &st);
  int c2 = stmblcan_bus_check(&bb, err, sizeof(err));
  ok(c1 == 0 && c2 < 0 && (strstr(err, "transmit buffers") || strstr(err, "receive buffers")),
     "budget: 1 drive + the I/O module fits; 2 drives + it (4 out, 3 in a period): refused", c2);
  printf("#   %s\n", err);

  // 10. a drive and the file-described I/O module on one bus
  static sim_bus_t sb6;
  static sim_chip_t ch6;
  static drive_t d6;
  static iomod_t io6;
  memset(&sb6, 0, sizeof(sb6));
  sim_chip_init(&ch6, &sb6);
  drive_init(&d6, &sb6, 1);
  io6       = (iomod_t){.sn = {.rx = iomod_rx}, .di = 0x3C, .ain0 = 1.65f, .temp = 36.6f, .ready = 1};
  io6.sn.ctx = &io6;
  sb6.node[sb6.nnode++] = &io6.sn;
  stmblcan_bus_t b6;
  memset(&b6, 0, sizeof(b6));
  b6.mcp.xfer = sim_xfer;
  b6.mcp.ctx  = &ch6;
  st.node     = 1;
  stmblcan_bus_add(&b6, &st);
  stmblcan_bus_add(&b6, &io);
  b6.node[0].vel_scale = 100.0f;
  int ck = stmblcan_bus_check(&b6, err, sizeof(err));
  int in6 = stmblcan_bus_init(&b6, 16000000, 1000000);
  b6.dev[0].val[1][0] = 0x81;   // do
  b6.dev[0].val[1][1] = 50.0f;  // pwm
  for(int k = 0; k < 200; k++) {
    b6.node[0].cmd.pos = k * 0.01f;
    stmblcan_bus_cycle(&b6);
    sim_bus_run(&sb6, 400.0);
    iomod_tick(&io6);  // its own clock, partway through the period
    sim_bus_run(&sb6, 600.0);
  }
  float *v6 = b6.dev[0].val[0];
  ok(ck == 0 && in6 == 0 && b6.dev[0].present && v6[0] == 0x3C && fabsf(v6[1] - 1.65f) < 0.001f &&
         fabsf(v6[2] - 36.6f) < 0.06f && v6[3] == 1,
     "the I/O module's status, decoded into its fields every period", v6[2]);
  uint16_t pwm_raw = (uint16_t)(io6.last_out.d[1] | io6.last_out.d[2] << 8);
  ok(io6.last_out.d[0] == 0x81 && fabsf(pwm_raw * 0.0015259f - 50.0f) < 0.002f && io6.outs >= 99 && io6.outs <= 101,
     "the I/O module's outputs sent every 2nd period", io6.outs);
  ok(b6.node[0].present && b6.node[0].missed == 0 && b6.foreign == 0 && b6.mcp.tx_late == 0,
     "the drive beside it: every feedback, nothing late or unknown", b6.node[0].missed);

  // 11. discovery: the drive hands out its description
  static sim_bus_t sb7;
  static sim_chip_t ch7;
  static drive_t d7;
  memset(&sb7, 0, sizeof(sb7));
  sim_chip_init(&ch7, &sb7);
  drive_init(&d7, &sb7, 9);
  d7.vel_scale = 250.0f;
  stmblcan_bus_t b7;
  memset(&b7, 0, sizeof(b7));
  b7.mcp.xfer = sim_xfer;
  b7.mcp.ctx  = &ch7;
  char dtext[128];
  int dr = stmblcan_bus_discover(&b7, 16000000, 1000000, 9, dtext, sizeof(dtext), wait_bus, &sb7);
  candesc_t found;
  int fp = dr == 0 ? candesc_parse(&found, dtext, err, sizeof(err)) : -1;
  ok(dr == 0 && fp == 0 && found.stmbl && fabsf(found.vel_scale - 250.0f) < 1e-3f,
     "discovery: node 9 answers with its description (stbl, stmbl1, vel_scale 250)", dr);
  found.node = 9;
  stmblcan_bus_add(&b7, &found);
  int i7 = stmblcan_bus_init(&b7, 16000000, 1000000);
  for(int k = 0; k < 50; k++) {
    b7.node[0].cmd.pos = 1.0f;
    stmblcan_bus_cycle(&b7);
    sim_bus_run(&sb7, 1000.0);
  }
  ok(i7 == 0 && b7.node[0].present && b7.node[0].fb.pos == 1.0f && b7.node[0].vel_scale == 250.0f,
     "  ...and runs with what it found", b7.node[0].fb.pos);
  char dtext2[128];
  int dr2 = stmblcan_bus_discover(&b7, 16000000, 1000000, 10, dtext2, sizeof(dtext2), wait_bus, &sb7);
  ok(dr2 == -2, "discovery of an absent node: no answer (-2)", dr2);

  // 12. the example description file, as stmblcan loads it
  FILE *ef = fopen("../linuxcnc/stmblcan/examples/bus0.desc", "r");
  static char etext[4096];
  size_t en = ef ? fread(etext, 1, sizeof(etext) - 1, ef) : 0;
  if(ef) {
    fclose(ef);
  }
  etext[en] = 0;
  candesc_t all[4];
  int cnt = 0;
  int el  = candesc_parse_all(etext, all, 4, &cnt, err, sizeof(err));
  ok(en > 0 && el == 0 && cnt == 2 && all[0].stmbl && all[0].node == 1 && !strcmp(all[1].name, "iomod") &&
         all[1].nframes == 2 && all[1].frame[1].every == 2,
     "examples/bus0.desc: a drive pinned down and the I/O module", el);
  int bl = candesc_parse_all("# two devices\ndevice a\nnode 3\nprofile stmbl1\ndevice b\nframe f 0x300 2 rx\n  x u20 0\n",
                             all, 4, &cnt, err, sizeof(err));
  ok(bl == 7 && strstr(err, "outside the frame"), "an error in the 2nd device: the line of the whole file", bl);

  printf("1..%d\n# %d/%d passed\n", n, n - fails, n);
  return fails ? 1 : 0;
}
