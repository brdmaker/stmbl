/*
 * This file is part of the stmbl project.
 *
 * Host simulation runner: builds the portable stmbl HAL core and control
 * components natively (x86), replacing the STM32 low-level layer (timer ISRs,
 * ADC, USB) with a software tick loop and stdin command console. This lets the
 * real control components run and be driven/inspected for automated testing.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hal.h"
#include "commands.h"
#include "usbd_cdc_if.h"
#include "drive.h"
#include "pty.h"
#include "sim_io.h"
#include "sscon.h"
#include "can_host.h"
#include "candesc.h"

#include <time.h>

extern volatile uint64_t systime;

// --- LinuxCNC side of the smart-serial link (drive mode) --------------------
// Stands in for the LinuxCNC servo thread: once per ms it sends the held
// command, and pos_cmd advances by vel_cmd (a constant-velocity jog). With
// an acceleration (lcnc EN VEL POS ACC) it moves as LinuxCNC's planner
// does, with constant acceleration: the true velocity ramps, the position
// follows it exactly, and vel_cmd is what LinuxCNC sends (joint.N.vel-cmd:
// the average over the last period).
static int lcnc_on = 0;
static int rt_count = 0;
static float lcnc_acc = 0.0f, lcnc_vel = 0.0f;

static sscon_t lcnc_con;      // LinuxCNC's end of the console (shared/sscon.h)
static char con_in[65536];    // what came, emptied continuously (as sserialterm into its pty)
static uint32_t con_in_len = 0;

static int lcnc_can = 0;  // 1: the link is CAN (lcnccan), LinuxCNC's end as stmblcan on a Pi

// index homing as LinuxCNC's motion does it (lcncindex): index-enable set,
// the driver (hm2, stmblcan) clears it once the drive found the index, and
// motion takes the position feedback of that same period as the index
// position, moving its command by the feedback's jump. The next periods'
// feedback must continue from there (a jump: the feedback that cleared
// index-enable was from before the drive switched to the index position),
// and the drive must keep following the moved command (the following
// error stays small).
static struct {
  int armed, periods, found;
  float at, prev, jump;  // the latched feedback; the largest jump after it
  float ferr;            // the largest |pos_cmd - pos_fb| after it
  float last;            // the feedback of the period before
  int after;
} lidx;

// once per servo period, with the feedback just read; returns how much
// motion moves its command (motor coordinates) at the index
static float lcnc_index(int cleared, float pos_fb, float pos_cmd, float vel) {
  float rebase = 0.0f;
  if(lidx.armed) {
    lidx.periods++;
    if(cleared) {
      lidx.armed = 0;
      lidx.found = 1;
      lidx.at = lidx.prev = pos_fb;
      lidx.after          = 0;
      lidx.jump = lidx.ferr = 0;
      rebase                = pos_fb - lidx.last - vel * 0.001f;
    }
  } else if(lidx.found && lidx.after < 1000) {  // the second after
    if(lidx.after < 20) {                        // continuous?
      lidx.jump = fmaxf(lidx.jump, fabsf(pos_fb - lidx.prev - vel * 0.001f));
    }
    lidx.ferr = fmaxf(lidx.ferr, fabsf(pos_cmd - pos_fb));
    lidx.prev = pos_fb;
    lidx.after++;
  }
  lidx.last = pos_fb;
  return rebase;
}

static void lcnc_servo(void) {
  sim_sserial_io_t *io = &sim_sserial_io;
  // a constant acceleration (lcnc ... acc): vel_cmd is the period's mean
  if(lcnc_acc != 0.0f) {
    float v0 = lcnc_vel;
    lcnc_vel += lcnc_acc * 0.001f;
    io->vel_cmd = 0.5f * (v0 + lcnc_vel);
  }
  if(lcnc_can) {
    // stmblcan.read first in the servo thread, stmblcan.write last
    stmblcan_node_t *n = can_host_node();
    can_host_read();
    io->pos_fb  = n->fb.pos;
    io->vel_fb  = n->fb.vel;
    io->current = n->slots[CANP_SLOT_CURRENT];
    io->power   = n->slots[CANP_SLOT_POWER];
    io->fault   = n->fb.fault || !n->present;
    if(n->index_found) {  // stmblcan clears index-enable
      io->index_enable = 0;
    }
    io->pos_cmd += lcnc_index(n->index_found, n->fb.pos, io->pos_cmd, io->vel_cmd);
    io->pos_cmd += io->vel_cmd * 0.001;
    n->cmd.pos          = io->pos_cmd;
    n->cmd.vel          = io->vel_cmd;
    n->cmd.enable       = io->enable;
    n->cmd.index_enable = io->index_enable;
    n->cmd.out = (uint8_t)(io->out[0] | io->out[1] << 1 | io->out[2] << 2 | io->out[3] << 3);
    n->cmd.con = sscon_frame(&lcnc_con, n->fb.con);  // as sserialterm on the con pins
    con_in_len += sscon_read(&lcnc_con, con_in + con_in_len, sizeof(con_in) - con_in_len);
    can_host_write();
    return;
  }
  // hm2: the drive no longer echoes index-enable (the answer to the
  // request of the period before, sent since)
  int cleared = lidx.armed && lidx.periods > 0 && io->index_enable && !io->index_enable_fb;
  if(cleared) {
    io->index_enable = 0;
  }
  io->pos_cmd += lcnc_index(cleared, io->pos_fb, io->pos_cmd, io->vel_cmd);
  io->pos_cmd += io->vel_cmd * 0.001;
  io->con_cmd = sscon_frame(&lcnc_con, io->con_fb);  // the last reply in, the next request out
  con_in_len += sscon_read(&lcnc_con, con_in + con_in_len, sizeof(con_in) - con_in_len);
  io->seq++;
}

static void run_cycles(long n) {
  for(long i = 0; i < n; i++) {
    if(lcnc_on && rt_count++ % RT_PER_NRT == 0) {
      lcnc_servo();
    }
    if(lcnc_can) {
      can_host_run(RT_PERIOD * 1e6);  // the bus during this rt period
    }
    drive_tick();
  }
}

// --- Host console commands (picked up by tools/create_cmd.py) ---------------
void cmd_step(char *ptr) {
  long n = 1;
  sscanf(ptr, " %li", &n);
  run_cycles(n);
}
COMMAND("step", cmd_step, "step <n> rt cycles (default 1, rt=0.2ms)");

void cmd_run(char *ptr) {
  float seconds = 0.0f;
  sscanf(ptr, " %f", &seconds);
  run_cycles((long)(seconds / RT_PERIOD));
}
COMMAND("run", cmd_run, "run <seconds> of simulated time");

void cmd_nrt(char *ptr) {
  (void)ptr;
  hal_run_nrt();
}
COMMAND("nrt", cmd_nrt, "run one non-realtime pass");

void cmd_time(char *ptr) {
  (void)ptr;
  printf("sim time: %.6f s (%llu ms)\n", drive_time(), (unsigned long long)systime);
}
COMMAND("time", cmd_time, "print simulated time");

void cmd_lcnc(char *ptr) {
  sim_sserial_io_t *io = &sim_sserial_io;
  int en;
  float vel, pos, acc;
  int n = sscanf(ptr, " %i %f %f %f", &en, &vel, &pos, &acc);
  if(n >= 1) {
    lcnc_on    = 1;
    io->enable = en > 0;
  }
  if(n >= 2) {
    io->vel_cmd = lcnc_vel = vel;
    lcnc_acc    = n >= 4 ? acc : 0.0f;
  }
  if(n >= 3) {
    io->pos_cmd = pos;
  }
  if(n < 1) {
    printf("lcnc: en %i pos_cmd %f vel_cmd %f pos_fb %f vel_fb %f fault %i\n",
           io->enable, io->pos_cmd, io->vel_cmd, io->pos_fb, io->vel_fb, io->fault);
  }
}
COMMAND("lcnc", cmd_lcnc, "LinuxCNC side: lcnc <enable> [vel_cmd [pos_cmd [acc]]], or show feedback");

void cmd_lcncindex(char *ptr) {
  int arm = -1;
  sscanf(ptr, " %i", &arm);
  if(arm == 1) {
    memset(&lidx, 0, sizeof(lidx));
    lidx.armed                  = 1;
    sim_sserial_io.index_enable = 1;
    return;
  }
  // as pin values, for tests' @expect
  printf("lcncindex.found = %d\nlcncindex.periods = %d\nlcncindex.at = %f\nlcncindex.jump = %f\nlcncindex.ferr = %f\n",
         lidx.found, lidx.periods, (double)lidx.at, (double)lidx.jump, (double)lidx.ferr);
}
COMMAND("lcncindex", cmd_lcncindex, "LinuxCNC index homing: lcncindex 1 arms; lcncindex: found, the jump after");

// the console over smart-serial, as LinuxCNC's sserialterm: send a line,
// print what came back
void cmd_con(char *ptr) {
  while(*ptr == ' ') {
    ptr++;
  }
  char line[256];
  int n = snprintf(line, sizeof(line), "%s\n", ptr);
  sscon_write(&lcnc_con, line, n);
}
COMMAND("con", cmd_con, "send a line over the smart-serial console: con <text>");

void cmd_conread(char *ptr) {
  (void)ptr;
  static char line[512];
  static int len = 0;
  for(uint32_t i = 0; i < con_in_len; i++) {
    if(con_in[i] == '\n' || len == (int)sizeof(line) - 1) {
      line[len] = 0;
      fprintf(stdout, "con> %s\n", line);
      len = 0;
    } else if(con_in[i] != '\r') {
      line[len++] = con_in[i];
    }
  }
  con_in_len = 0;
  fprintf(stdout, "con: %u bytes in, %u out\n", lcnc_con.n_rx, lcnc_con.n_tx);
}
COMMAND("conread", cmd_conread, "print what came over the smart-serial console");

// the link is CAN (docs/can_support.md): LinuxCNC's end as stmblcan on a Pi
// (its bus code and MCP2515 driver, on a simulated MCP2515), the drive's
// can component on the same simulated bus. lcnc / con / conread / lcncstop
// work as with smart-serial. The drive config links "can" for "sserial".
static int can_discovering;
static void can_wait(void *ctx, int us) {  // discovery: the drive and the bus run
  (void)ctx;
  for(int t = 0; t < us; t += (int)(RT_PERIOD * 1e6)) {
    can_host_run(RT_PERIOD * 1e6);
    drive_tick();
  }
}

void cmd_lcnccan(char *ptr) {
  int node = 1;
  float vs = 100.0f;
  if(sscanf(ptr, " discover %i", &node) == 1) {
    // as stmblcan with discover=1: ask the drive, then run with what it says
    char text[160], err[96];
    candesc_t d;
    can_discovering = 1;
    int r           = can_host_discover(node, text, sizeof(text), can_wait);
    can_discovering = 0;
    if(r < 0) {
      printf("lcnccan: node %d: no answer\n", node);
      return;
    }
    int l = candesc_parse(&d, text, err, sizeof(err));
    printf("lcnccan: node %d describes itself:\n%s", node, text);
    if(l || !d.stmbl) {
      printf("lcnccan: not a stmbl drive (%s)\n", l ? err : "profile");
      return;
    }
    vs = d.vel_scale;
  } else {
    sscanf(ptr, " %i %f", &node, &vs);
  }
  if(can_host_init(node, vs)) {
    printf("lcnccan: the simulated MCP2515 did not initialise\n");
    return;
  }
  sscon_reset(&lcnc_con);
  lcnc_can = 1;
  printf("lcnccan: node %d, vel_scale %g, 1 Mbit/s\n", node, (double)vs);
}
COMMAND("lcnccan", cmd_lcnccan, "LinuxCNC side over CAN: lcnccan [node [vel_scale]] | lcnccan discover node");

void cmd_canstat(char *ptr) {
  (void)ptr;
  can_host_stat();
}
COMMAND("canstat", cmd_canstat, "CAN link counters (lcnccan)");

void cmd_canplug(char *ptr) {
  int on = 1;
  sscanf(ptr, " %i", &on);
  can_host_drive_attached(on);
}
COMMAND("canplug", cmd_canplug, "canplug 0: the drive off the CAN bus, 1: back on");

// the link goes away (cable, LinuxCNC stopped): no more frames; the drive
// times out. 'lcnc ...' starts them again.
void cmd_lcncstop(char *ptr) {
  (void)ptr;
  lcnc_on = 0;
}
COMMAND("lcncstop", cmd_lcncstop, "LinuxCNC side: stop sending smart-serial frames");

// LinuxCNC's end of the console starts afresh (sserialterm loaded again)
void cmd_conreset(char *ptr) {
  (void)ptr;
  sscon_reset(&lcnc_con);
  con_in_len = 0;
}
COMMAND("conreset", cmd_conreset, "LinuxCNC side: restart the console's link layer");

void cmd_exit(char *ptr) {
  (void)ptr;
  exit(0);
}
COMMAND("exit", cmd_exit, "quit the simulator");
COMMAND("quit", cmd_exit, "quit the simulator");

// Clean one input line in place and run it; ignores comments and blank lines.
static void run_line(char *line) {
  // strip trailing newline so hal_parse sees no dangling empty segment
  size_t len = strlen(line);
  while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
    line[--len] = 0;
  }
  // strip inline comments so scripts can be annotated
  char *hash = strchr(line, '#');
  if(hash) {
    *hash = 0;
  }
  // skip blank / whitespace-only lines
  char *p = line;
  while(*p == ' ' || *p == '\t') {
    p++;
  }
  if(*p == 0) {
    return;
  }
  hal_parse(line);
}

// Deterministic script mode: read commands from a file or stdin and run them.
// Time only advances via the explicit 'step'/'run' commands.
static int run_script(FILE *in) {
  char line[256];
  while(fgets(line, sizeof(line), in)) {
    run_line(line);
    if(in == stdin) {
      fflush(stdout);  // a program driving us over a pipe waits for the reply
    }
  }
  return 0;
}

// Serve mode: speak the device's wire protocol. 'term' is loaded so its scope
// stream (0xFF frames) and text responses go to stdout, exactly as on the USB
// serial port. Commands arrive on stdin. Deterministic: time advances only via
// 'run'/'step', and nrt (scope drain) is interleaved while it does.
static int serve(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  drive_set_nrt(1);
  cdc_host_set_mode(CDC_STDIO);
  cdc_host_set_connected(1);
  load_comp(comp_by_name("term"));  // term0: the comms / scope channel

  char line[256];
  while(fgets(line, sizeof(line), stdin)) {
    run_line(line);
    hal_run_nrt();  // flush any residual scope and refresh nrt state
  }
  return 0;
}

// Drive mode, in real time: the console is on a pty, and time advances by
// itself at 1 ms per ms. Nothing plays LinuxCNC, so the drive shows a command
// error, as a real drive does with LinuxCNC not running.
static int drive_realtime(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  for(;;) {
    for(int i = 0; i < RT_PER_NRT; i++) {
      drive_tick();
    }
    t.tv_nsec += 1000000;
    if(t.tv_nsec >= 1000000000) {
      t.tv_nsec -= 1000000000;
      t.tv_sec++;
    }
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);
  }
  return 0;
}

static void usage(void) {
  fprintf(stderr,
          "usage: stmbl_host [SCRIPT]                 run a script (or stdin)\n"
          "       stmbl_host --serve                  device wire protocol on stdin/stdout\n"
          "       stmbl_host --drive CONF [--sim FILE] [--pty LINK] [SCRIPT]\n"
          "  --drive CONF  boot a simulated drive from CONF, its flash config\n"
          "                (with --pty, flashsaveconf writes back to CONF)\n"
          "  --sim FILE    sim-only settings, parsed after host/sim_board.txt\n"
          "  --pty LINK    real time, console on a pty symlinked at LINK;\n"
          "                without it, SCRIPT drives the clock (step/run)\n");
}

// --- Main -------------------------------------------------------------------
int main(int argc, char **argv) {
  hal_init(RT_PERIOD, FRT_PERIOD);
  hal_set_debug_level(0);

  const char *script = NULL;
  const char *conf   = NULL;
  const char *sim    = NULL;
  const char *pty    = NULL;
  int do_serve       = 0;
  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "--serve") == 0) {
      do_serve = 1;
    } else if(strcmp(argv[i], "--drive") == 0 && i + 1 < argc) {
      conf = argv[++i];
    } else if(strcmp(argv[i], "--sim") == 0 && i + 1 < argc) {
      sim = argv[++i];
    } else if(strcmp(argv[i], "--pty") == 0 && i + 1 < argc) {
      pty = argv[++i];
    } else if(argv[i][0] == '-' && argv[i][1]) {
      usage();
      return 1;
    } else {
      script = argv[i];
    }
  }

  if(do_serve) {
    return serve();
  }

  if(conf) {
    char *text = sim_read_file(conf);
    if(!text) {
      fprintf(stderr, "cannot read %s\n", conf);
      return 1;
    }
    drive_set_flash(text);
    free(text);
    if(sim) {
      char *extra = sim_read_file(sim);
      if(!extra) {
        fprintf(stderr, "cannot read %s\n", sim);
        return 1;
      }
      drive_set_sim_extra(extra);
      free(extra);
    }
    drive_set_nrt(1);
    if(pty) {
      if(sim_io_start(pty, conf) < 0) {
        return 1;
      }
      drive_boot();
      fprintf(stderr, "simulated drive on %s\n", pty);
      return drive_realtime();
    }
    drive_boot();
  }

  FILE *in = stdin;
  if(script) {
    in = fopen(script, "r");
    if(!in) {
      fprintf(stderr, "cannot open %s\n", script);
      return 1;
    }
  }
  int rc = run_script(in);
  if(in != stdin) {
    fclose(in);
  }
  return rc;
}
