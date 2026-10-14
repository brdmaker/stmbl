/*
 * The simulated drive (host/drive.c): boot, clock and simulated flash.
 */
#pragma once

// rt = 5 kHz control loop, frt = 20 kHz fast loop (same rates as the firmware).
#define RT_PERIOD 0.0002f
#define FRT_PERIOD 0.00005f
#define FRT_PER_RT 4  // FRT_PERIOD * FRT_PER_RT == RT_PERIOD
#define RT_PER_NRT 5  // device runs nrt at ~1 kHz, rt at 5 kHz

void drive_tick(void);       // one rt cycle (and its frt cycles)
void drive_set_nrt(int on);  // also run nrt every RT_PER_NRT cycles
double drive_time(void);     // simulated time [s]

// boot like src/main.c: term, flashloadconf, loadconf, sim_board, relink, start
void drive_boot(void);
void drive_poll_reset(void);  // reboot if a 'reset' command asked for it

void drive_set_flash(const char *text);  // contents of the config flash
const char *drive_flash(void);
unsigned drive_flash_version(void);       // bumped by every flashsaveconf
void drive_set_sim_extra(const char *text);  // parsed after sim_board
