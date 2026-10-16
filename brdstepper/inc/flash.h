#pragma once
#include <stdint.h>

// brdstepper/src/flash.c: the flash layout (dual bank, 2 kB pages)
#define FLASH_BOOT 0x08000000u    // bootloader, 16 kB
#define FLASH_APP 0x08004000u     // application, 240 kB
#define FLASH_BANK2 0x08040000u
#define FLASH_STAGE 0x08040000u   // update staging, 240 kB
#define FLASH_STAGE_SIZE (240u * 1024u)
#define FLASH_CONF 0x0807C000u    // config, 16 kB
#define FLASH_CONF_SIZE (16u * 1024u)
#define FLASH_END 0x08080000u
#define FLASH_PAGE 2048u

int flash_erase_page(uint32_t addr);                              // bank 2 only
int flash_program(uint32_t addr, const void *data, uint32_t len);  // 8-byte aligned, multiples of 8
