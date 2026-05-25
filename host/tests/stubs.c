/* Host stubs for unit-test binaries.
 *
 * The test binaries link with the HAL + all components but without main.c.
 * This file supplies the symbols that main.c would otherwise provide:
 *   - hardware timing stubs (same as main.c)
 *   - systime global referenced by commands.c
 *   - no-op wrappers for host commands registered via COMMAND() in main.c
 *     so that the generated commandslist.h links cleanly
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Referenced by commands.c uptime() via "extern uint64_t systime". */
uint64_t systime = 0;

/* Low-level hardware stubs — same values as main.c. */
uint32_t hal_get_systick_value(void)  { return 0; }
uint32_t hal_get_systick_reload(void) { return 0xFFFFFF; }
uint32_t hal_get_systick_freq(void)   { return 168000000; }
void hal_init_watchdog(float t)       { (void)t; }
void hal_reset_watchdog(void)         {}
void Wait(uint32_t ms)                { (void)ms; }

/* Host command stubs: registered via COMMAND() in main.c and referenced by
 * gen/commandslist.h.  Not exercised by tests but must link. */
void cmd_step(char *ptr)  { (void)ptr; }
void cmd_run(char *ptr)   { (void)ptr; }
void cmd_nrt(char *ptr)   { (void)ptr; }
void cmd_time(char *ptr)  { (void)ptr; }
void cmd_exit(char *ptr)  { (void)ptr; exit(0); }
