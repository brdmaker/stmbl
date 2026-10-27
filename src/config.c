/*
* This file is part of the stmbl project.
*
* Copyright (C) 2013-2015 Rene Hopf <renehopf@mac.com>
* Copyright (C) 2013-2015 Nico Stute <crinq@crinq.de>
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

#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "commands.h"
#include "stm32f4xx_conf.h"

char config[15 * 1024];
const char *config_ro = (char *)0x08008000;

void confcrc(char *ptr) {
  uint32_t len = strnlen(config, sizeof(config) - 1);
  CRC_ResetDR();
  uint32_t crc = CRC_CalcBlockCRC((uint32_t *)config, len / 4);
  for(int i = 0; i < len; i++) {
    printf("%x ", config[i]);
  }
  printf("\n");
  printf("size: %lu words: %lu crc:%lx\n", len, len / 4, crc);
}
COMMAND("confcrc", confcrc, "foo");

const char *conf_flash_text(void) {  // shared/commands.c: version, the boot check
  return config_ro;
}

char *conf_ram(size_t *size) {  // shared/commands.c: name, motor
  *size = sizeof(config);
  return config;
}

void flashloadconf(char *ptr) {
  strncpy(config, config_ro, sizeof(config));
}
COMMAND("flashloadconf", flashloadconf, "load config from flash");

// A sector erase stalls every flash fetch, interrupts included, for 0.25 s
// or more. rt then misses its deadline and the drive stops with RT_TOO_LONG
// (4 of 5 saves on the bench). So stop rt/frt around the write and start
// them again afterwards, and refuse while the drive is enabled.
static int drive_enabled(void) {
  NAME comp = "fault", pin = "en_out";
  hal_pin_inst_t *en = pin_inst_by_name(comp, 0, pin);
  return en && en->source->value > 0.0;
}

void flashsaveconf(char *ptr) {
  if(drive_enabled()) {
    printf("error: drive enabled, disable it before flashsaveconf\n");
    return;
  }
  int restart = hal.hal_state == HAL_OK2 && hal.rt_state != RT_STOP;
  hal_stop();
  printf("erasing flash page...\n");
  FLASH_Unlock();
  if(FLASH_EraseSector(FLASH_Sector_2, VoltageRange_3) != FLASH_COMPLETE) {
    printf("error!\n");
    FLASH_Lock();
    if(restart) {
      hal_start();
    }
    return;
  }
  printf("saving conf\n");
  int i   = 0;
  int ret = 0;
  do {
    ret = FLASH_ProgramByte((uint32_t)(config_ro + i), config[i]) != FLASH_COMPLETE;
    if(ret) {
      printf("error writing %i\n", ret);
      break;
    }
  } while(config[i++] != 0);
  FLASH_Lock();
  if(restart) {
    hal_start();
  }
  printf("OK %i bytes written\n", i);
}
COMMAND("flashsaveconf", flashsaveconf, "save config to flash");

void loadconf(char *ptr) {
  hal_parse(config);
}
COMMAND("loadconf", loadconf, "parse config");

void showconf(char *ptr) {
  printf("%s", config_ro);
}
COMMAND("showconf", showconf, "show config");

void appendconf(char *ptr) {
  printf("adding %s\n", ptr);
  strncat(config, ptr, sizeof(config) - 1);
  strncat(config, "\n", sizeof(config) - 1);
}
COMMAND("appendconf", appendconf, "append string to config");

void deleteconf(char *ptr) {
  config[0] = '\0';
}
COMMAND("deleteconf", deleteconf, "delete config");

void hardboot(char *ptr) {
  if(drive_enabled()) {
    printf("error: drive enabled, disable it before hardboot\n");
    return;
  }
  hal_stop();  // the erase stalls rt, see flashsaveconf
  printf("erasing flash page, then reset into the bootloader\n");
  // Sector 4 holds the vector table (0x08010000): an interrupt after the
  // erase jumped into blank flash and locked the core up (USB still
  // enumerated, console dead, until a power cycle). No interrupts from
  // here on, and reset in any case: the stmbl bootloader checks the CRC.
  __disable_irq();
  FLASH_Unlock();
  FLASH_EraseSector(FLASH_Sector_4, VoltageRange_3);
  FLASH_Lock();
  NVIC_SystemReset();
}
COMMAND("hardboot", hardboot, "destroy firmware to force bootloader");