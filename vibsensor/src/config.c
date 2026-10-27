/*
 * The sensor's config text, as brdstepper's and the F4's (same commands:
 * showconf, appendconf, flashsaveconf, ...), at the end of flash bank 2
 * (vibsensor/inc/flash.h). Parameters set over the bus's console are kept
 * by appending them and saving. An erased (never saved) config links the
 * default template, vibsensor/conf/vibsensor.txt.
 */
#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "commands.h"
#include "flash.h"

char config[6 * 1024];  // a sensor's config is short
static const char *config_ro = (const char *)FLASH_CONF;

void flashloadconf(char *ptr) {
  (void)ptr;
  if((uint8_t)config_ro[0] == 0xff) {
    config[0] = 0;  // never saved
    return;
  }
  strncpy(config, config_ro, sizeof(config) - 1);
  config[sizeof(config) - 1] = 0;
}
COMMAND("flashloadconf", flashloadconf, "load config from flash");

void flashsaveconf(char *ptr) {
  (void)ptr;
  uint32_t len = strnlen(config, sizeof(config) - 1) + 1;  // with the NUL
  for(uint32_t a = FLASH_CONF; a < FLASH_CONF + FLASH_CONF_SIZE; a += FLASH_PAGE) {
    if(flash_erase_page(a)) {
      printf("error erasing %08lx\n", (unsigned long)a);
      return;
    }
  }
  static uint8_t pad[8];
  uint32_t full = len & ~7u;
  if(flash_program(FLASH_CONF, config, full)) {
    printf("error writing\n");
    return;
  }
  memset(pad, 0xff, sizeof(pad));
  memcpy(pad, config + full, len - full);
  if(len > full && flash_program(FLASH_CONF + full, pad, 8)) {
    printf("error writing\n");
    return;
  }
  printf("OK %lu bytes written\n", (unsigned long)len);
}
COMMAND("flashsaveconf", flashsaveconf, "save config to flash");

void loadconf(char *ptr) {
  (void)ptr;
  if(config[0] == 0) {
    hal_parse("link vibsensor");  // vibsensor/conf/vibsensor.txt
    return;
  }
  hal_parse(config);
}
COMMAND("loadconf", loadconf, "parse config");

void showconf(char *ptr) {
  (void)ptr;
  if((uint8_t)config_ro[0] != 0xff) {
    printf("%s", config_ro);
  }
}
COMMAND("showconf", showconf, "show config");

void appendconf(char *ptr) {
  printf("adding %s\n", ptr);
  strncat(config, ptr, sizeof(config) - strlen(config) - 2);
  strncat(config, "\n", sizeof(config) - strlen(config) - 1);
}
COMMAND("appendconf", appendconf, "append string to config");

void deleteconf(char *ptr) {
  (void)ptr;
  config[0] = '\0';
}
COMMAND("deleteconf", deleteconf, "delete config");
