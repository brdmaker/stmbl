/*
 * The simulated drive's serial port and flash file (host/pty.c).
 */
#pragma once

// Serve the drive's console on a pty symlinked at pty_link (NULL: none) and
// write the simulated flash to flash_file (NULL: never) after flashsaveconf.
int sim_io_start(const char *pty_link, const char *flash_file);
void sim_io_stop(void);

// Whole file as a malloc'd string, or NULL.
char *sim_read_file(const char *path);
