/*
 * The drive's CAN controller, as the can component (shared/comps/can.c)
 * sees it: classic frames in and out, non-blocking. Per platform: the host
 * simulator (host/can_host.c, a node on a simulated bus); bxCAN on the F4
 * and FDCAN on brdstepper (to do).
 */
#pragma once
#include "canproto.h"

// accept only these identifiers (the controller's filters); n 0: any
void can_hw_filter(const uint16_t *ids, int n);
// a received frame into f: 1, none waiting: 0
int can_hw_recv(can_frame_t *f);
// queue a frame for sending: 0 ok, < 0 no room
int can_hw_send(const can_frame_t *f);
