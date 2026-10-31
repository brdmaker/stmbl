/*
 * This file is part of the stmbl project.
 *
 * The simulated drive: what src/main.c, src/config.c and the timer
 * interrupts do on the STM32, for the host build. It boots from a simulated
 * flash config exactly as the drive does (flashloadconf, loadconf, relink,
 * start), then wires the simulated hardware with the "sim_board" template.
 * Time advances only through drive_tick(), one 5 kHz rt cycle at a time, so
 * the caller (host/main.c or the LinuxCNC module) owns the clock.
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
#include <string.h>

#include "hal.h"
#include "commands.h"
#include "drive.h"
#include "version_number.h"

// 1 kHz millisecond counter that the firmware exposes via SysTick.
volatile uint64_t systime = 0;
static double sim_time_s  = 0.0;
static int nrt_divider    = 0;
static int run_nrt        = 0;

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

// --- Clock ------------------------------------------------------------------
void drive_set_nrt(int on) {
  run_nrt = on;
}

void drive_tick(void) {
  for(int i = 0; i < FRT_PER_RT; i++) {
    hal_run_frt();
    sim_time_s += FRT_PERIOD;
  }
  hal_run_rt();
  systime = (uint64_t)(sim_time_s * 1000.0);

  // the drive's main loop runs nrt about once per ms
  if(run_nrt && ++nrt_divider >= RT_PER_NRT) {
    nrt_divider = 0;
    hal_run_nrt();
    drive_poll_reset();
  }
}

double drive_time(void) {
  return sim_time_s;
}

// --- Flash config (src/config.c) ----------------------------------------------
// flash[] stands in for the config sector at 0x08008000, config[] for the RAM
// copy that appendconf builds and loadconf parses.
static char flash[15 * 1024];
static char config[15 * 1024];
static _Atomic unsigned flash_version = 0;

void drive_set_flash(const char *text) {
  strncpy(flash, text, sizeof(flash) - 1);
}

const char *drive_flash(void) {
  return flash;
}

unsigned drive_flash_version(void) {
  return flash_version;
}

const char *conf_flash_text(void) {  // shared/commands.c: version, the boot check
  return flash;
}

char *conf_ram(size_t *size) {  // shared/commands.c: name, motor
  *size = sizeof(config);
  return config;
}

void flashloadconf(char *ptr) {
  strncpy(config, flash, sizeof(config));
}
COMMAND("flashloadconf", flashloadconf, "load config from flash");

void flashsaveconf(char *ptr) {
  printf("erasing flash page...\n");
  printf("saving conf\n");
  size_t i = strnlen(config, sizeof(config) - 1);
  memcpy(flash, config, i);
  flash[i++] = 0;
  flash_version++;
  printf("OK %i bytes written\n", (int)i);
}
COMMAND("flashsaveconf", flashsaveconf, "save config to flash");

void loadconf(char *ptr) {
  hal_parse(config);
}
COMMAND("loadconf", loadconf, "parse config");

void showconf(char *ptr) {
  printf("%s", flash);
}
COMMAND("showconf", showconf, "show config");

void appendconf(char *ptr) {
  printf("adding %s\n", ptr);
  strncat(config, ptr, sizeof(config) - strlen(config) - 1);
  strncat(config, "\n", sizeof(config) - strlen(config) - 1);
}
COMMAND("appendconf", appendconf, "append string to config");

void deleteconf(char *ptr) {
  config[0] = '\0';
}
COMMAND("deleteconf", deleteconf, "delete config");

// --- Boot / reset (src/main.c) ------------------------------------------------
static char sim_extra[4096];
static int reset_pending = 0;

void drive_set_sim_extra(const char *text) {
  strncpy(sim_extra, text, sizeof(sim_extra) - 1);
}

void drive_boot(void) {
  hal_stop();
  hal_init(RT_PERIOD, FRT_PERIOD);
  NAME term = "term";
  load_comp(comp_by_name(term));
  hal_parse("flashloadconf");
  hal_parse("loadconf");
  conf_version_check();
  // sim only: the simulated motor and encoder follow the config
  hal_parse("link sim_board");
  if(sim_extra[0]) {
    // line by line: a '#' comment would otherwise swallow the rest of the file
    char buf[sizeof(sim_extra)];
    strcpy(buf, sim_extra);
    for(char *line = strtok(buf, "\n"); line; line = strtok(NULL, "\n")) {
      char *hash = strchr(line, '#');
      if(hash) {
        *hash = 0;
      }
      if(strspn(line, " \t\r") < strlen(line)) {
        hal_parse(line);
      }
    }
  }
  hal_parse("relink");
  hal_parse("start");
}

void nv_reset(char *ptr) {
  reset_pending = 1;  // reboot once the command that asked for it returned
}
COMMAND("reset", nv_reset, "reset STMBL");

void drive_poll_reset(void) {
  if(reset_pending) {
    reset_pending = 0;
    drive_boot();
  }
}

void about(char *ptr) {
  printf("######## software info ########\n");
  printf("STMBL simulated drive (host/drive.c)\n");
  printf("firmware v%d\n", STMBL_FW_VERSION);
  printf("sim time: %.3f s\n", sim_time_s);
}
COMMAND("about", about, "show system infos");
