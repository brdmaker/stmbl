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

#include <time.h>

extern volatile uint64_t systime;

// --- LinuxCNC side of the smart-serial link (drive mode) --------------------
// Stands in for the LinuxCNC servo thread: once per ms it sends the held
// command, and pos_cmd advances by vel_cmd (a constant-velocity jog).
static int lcnc_on = 0;
static int rt_count = 0;

static void lcnc_servo(void) {
  sim_sserial_io_t *io = &sim_sserial_io;
  io->pos_cmd += io->vel_cmd * 0.001;
  io->seq++;
}

static void run_cycles(long n) {
  for(long i = 0; i < n; i++) {
    if(lcnc_on && rt_count++ % RT_PER_NRT == 0) {
      lcnc_servo();
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
  float vel, pos;
  int n = sscanf(ptr, " %i %f %f", &en, &vel, &pos);
  if(n >= 1) {
    lcnc_on    = 1;
    io->enable = en > 0;
  }
  if(n >= 2) {
    io->vel_cmd = vel;
  }
  if(n >= 3) {
    io->pos_cmd = pos;
  }
  if(n < 1) {
    printf("lcnc: en %i pos_cmd %f vel_cmd %f pos_fb %f vel_fb %f fault %i\n",
           io->enable, io->pos_cmd, io->vel_cmd, io->pos_fb, io->vel_fb, io->fault);
  }
}
COMMAND("lcnc", cmd_lcnc, "LinuxCNC side: lcnc <enable> [vel_cmd [pos_cmd]], or show feedback");

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
