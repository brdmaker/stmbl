#pragma once
/*
 * LBP host-side client library: implements the Mesa Smart Serial Local Bus
 * Protocol (LBP) host role.  Use with a file descriptor pointing to a serial
 * port, PTY, or socketpair connected to an LBP slave.
 */
#include <stdint.h>
#include "sserial.h"  /* discovery_rpc_t, lbp_t, protocol constants */

typedef struct {
  int fd;
  int timeout_ms;
  /* Optional callback invoked after each transmit and before the receive.
     In the host simulation this is used to call "step N" on stmbl_host so
     frt_func runs and processes the bytes we just sent. */
  void (*advance_fn)(void *arg);
  void *advance_arg;
} lbp_conn_t;

/* Initialise a connection over an already-open file descriptor.
   advance_fn / advance_arg default to NULL (no callback). */
void lbp_conn_init(lbp_conn_t *c, int fd, int timeout_ms);

/* CT_LOCAL reads -----------------------------------------------------------*/

/* Issue LBPCookieCMD; return 1 if slave returns 0x5A, 0 on failure. */
int lbp_check_cookie(lbp_conn_t *c);

/* Read the four-byte card-name via LBPCardName0..3Cmd.  out[] is NOT
   NUL-terminated (it is exactly 4 bytes).  Returns 1 on success. */
int lbp_read_card_name(lbp_conn_t *c, char out[4]);

/* CT_RPC commands ----------------------------------------------------------*/

/* UnitNumberRPC (0xBC): read 32-bit unit number.  Returns 1 on success. */
int lbp_read_unit_number(lbp_conn_t *c, uint32_t *unit);

/* DiscoveryRPC (0xBB): fill *disc with input/output sizes and PTOC/GTOC
   pointers.  Returns 1 on success. */
int lbp_discover(lbp_conn_t *c, discovery_rpc_t *disc);

/* CT_RW memory access ------------------------------------------------------*/

/* Read (1<<ds) bytes from the slave's address space.  ds: 0=1B 1=2B 2=4B
   3=8B.  addr is always sent explicitly (AddressSize=1).  Returns 1 on
   success and fills *data with (1<<ds) bytes. */
int lbp_read_mem(lbp_conn_t *c, uint16_t addr, uint8_t ds, void *data);

/* Cyclic process-data exchange ---------------------------------------------*/

/* ProcessDataRPC (0xBD): send disc->output bytes from out_data to the slave,
   receive disc->input bytes (1 fault byte + (disc->input-1) data bytes).
   fault_out may be NULL.  in_data receives disc->input-1 bytes.
   Returns 1 on success (CRC verified), 0 on timeout or CRC error. */
int lbp_process_data(lbp_conn_t *c, const discovery_rpc_t *disc,
                     const void *out_data, uint8_t *fault_out, void *in_data);
