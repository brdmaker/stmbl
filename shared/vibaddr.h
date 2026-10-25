/*
 * Node numbers on the measurement bus without jumpers (docs/vibsensor.md
 * §3.1): a sensor is known by its MCU's 96-bit unique ID. The PC finds every
 * sensor by a binary search over those IDs, then gives each a node number
 * (1..15) and a name, which the sensor keeps in its flash. A sensor without
 * a number (node 0) sends nothing but these replies.
 *
 * Frames, on two IDs at the bottom of the bus's priorities:
 *   0x7E5  PC -> sensors       0x7E4  sensors -> PC
 *
 *   SCAN      [0x10, scope, n, prefix(12)]  every sensor in scope (0: those
 *             without a number, 1: all) that isn't muted and whose ID begins
 *             with the n bits of prefix (MSB first) answers YES [0x11]. All
 *             answers are the same frame, so they may overlap on the bus.
 *   QUERY     [0x20, uid]                   INFO [0x21, uid, node, fw(2), name(16)]
 *   SET       [0x22, uid, node, name(16), what]  what: 1 node, 2 name;
 *             answered by INFO with 0x23
 *   STORE     [0x24, uid]                   into flash; STORED [0x25, uid, node]
 *   MUTE      [0x26, uid, on]               out of the SCANs (found already)
 *   UNMUTE    [0x27]                        all back in
 *   IDENTIFY  [0x28, on]                    while on, a sensor that is tapped
 *             answers TAP [0x29, uid, node]
 * A sensor answers only what names its uid (or SCAN, UNMUTE, IDENTIFY).
 */
#pragma once
#include <stdint.h>
#include "vibproto.h"

#define VIBA_CMD_ID 0x7E5
#define VIBA_RSP_ID 0x7E4
#define VIBA_UID 12
#define VIBA_NAME 16

enum {
  VIBA_SCAN     = 0x10,
  VIBA_YES      = 0x11,
  VIBA_QUERY    = 0x20,
  VIBA_INFO     = 0x21,
  VIBA_SET      = 0x22,
  VIBA_SET_OK   = 0x23,
  VIBA_STORE    = 0x24,
  VIBA_STORED   = 0x25,
  VIBA_MUTE     = 0x26,
  VIBA_UNMUTE   = 0x27,
  VIBA_IDENTIFY = 0x28,
  VIBA_TAP      = 0x29,
};
enum { VIBA_SET_NODE = 1, VIBA_SET_NAME = 2 };

typedef struct {
  uint8_t uid[VIBA_UID];       // the MCU's unique ID
  uint8_t node;                // 0: none yet
  char name[VIBA_NAME];        // "spindle", "X axis"; not 0-terminated when full
  uint16_t fw;                 // firmware version (STMBL_FW_VERSION)
  uint8_t muted, identify;
  uint8_t store_req;           // set by STORE: the firmware writes node and name to flash
} vibaddr_t;

// A frame from the PC; returns 1 and fills out if the sensor answers.
int vibaddr_rx(vibaddr_t *a, const vibp_frame_t *in, vibp_frame_t *out);
// The sensor was tapped (vib0.taps counted up); returns 1 and fills out
// while IDENTIFY is on.
int vibaddr_tap(const vibaddr_t *a, vibp_frame_t *out);
