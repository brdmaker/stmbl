#pragma once
#include <stdint.h>
#include <stddef.h>

#define COMMAND(c, p, d)

uint32_t call_cmd(char *s);
const char *cmd_name(int32_t i);  // NULL: no such command
extern volatile int32_t cmd_active;

typedef struct {
  char name[32];
  char *doc;
  void (*ptr)(char *);
} cmd_t;

void conf_version_check(void);   // shared/commands.c: at boot, after the config
const char *conf_flash_text(void);  // the flash config's text (weak: none)
char *conf_ram(size_t *size);       // the RAM config flashsaveconf writes (weak: none)
