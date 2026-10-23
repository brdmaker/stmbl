#pragma once
#include <stdint.h>

#define COMMAND(c, p, d)

uint32_t call_cmd(char *s);

typedef struct {
  char name[32];
  char *doc;
  void (*ptr)(char *);
} cmd_t;

void conf_version_check(void);   // shared/commands.c: at boot, after the config
const char *conf_flash_text(void);  // the flash config's text (weak: none)
