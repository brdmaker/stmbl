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

// rt = 5 kHz control loop, frt = 20 kHz fast loop (same rates as the firmware).
#define RT_PERIOD 0.0002f
#define FRT_PERIOD 0.00005f
#define FRT_PER_RT 4  // FRT_PERIOD * FRT_PER_RT == RT_PERIOD

// 1 kHz millisecond counter that the firmware exposes via SysTick.
volatile uint64_t systime = 0;
static double sim_time_s  = 0.0;

// --- Low-level hardware stubs -----------------------------------------------
// On the STM32 these read the SysTick peripheral; here we only need a non-zero
// frequency so the HAL's timing-statistics math does not divide by zero.
uint32_t hal_get_systick_value(void) {
  return 0;
}
uint32_t hal_get_systick_reload(void) {
  return 0xFFFFFF;
}
uint32_t hal_get_systick_freq(void) {
  return 168000000;  // matches the F4 HCLK
}
void hal_init_watchdog(float time) {
  (void)time;
}
void hal_reset_watchdog(void) {
}
void Wait(uint32_t ms) {
  (void)ms;
}

// --- Software tick loop -----------------------------------------------------
static void tick_rt_cycle(void) {
  for(int i = 0; i < FRT_PER_RT; i++) {
    hal_run_frt();
    sim_time_s += FRT_PERIOD;
  }
  hal_run_rt();
  systime = (uint64_t)(sim_time_s * 1000.0);
}

static void run_cycles(long n) {
  for(long i = 0; i < n; i++) {
    tick_rt_cycle();
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
  printf("sim time: %.6f s (%llu ms)\n", sim_time_s, (unsigned long long)systime);
}
COMMAND("time", cmd_time, "print simulated time");

void cmd_exit(char *ptr) {
  (void)ptr;
  exit(0);
}
COMMAND("exit", cmd_exit, "quit the simulator");
COMMAND("quit", cmd_exit, "quit the simulator");

// --- Main -------------------------------------------------------------------
int main(int argc, char **argv) {
  hal_init(RT_PERIOD, FRT_PERIOD);
  hal_set_debug_level(0);

  FILE *in = stdin;
  if(argc > 1) {
    in = fopen(argv[1], "r");
    if(!in) {
      fprintf(stderr, "cannot open %s\n", argv[1]);
      return 1;
    }
  }

  char line[256];
  while(fgets(line, sizeof(line), in)) {
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
      continue;
    }
    hal_parse(line);
  }

  if(in != stdin) {
    fclose(in);
  }
  return 0;
}
