/*
 * What the vnode component (the vibration sensor on its measurement bus,
 * shared/comps/vnode.c) needs of its platform: the FDCAN controller on the
 * sensor (vibsensor/src/fdcan.c), a simulated bus in the host simulator
 * (host/vnode_host.c).
 */
#pragma once
#include <stdint.h>
#include "vibproto.h"

int vnode_hw_recv(vibp_frame_t *f);        // a received frame: 1, none: 0
int vnode_hw_send(const vibp_frame_t *f);  // queued: 0, no room: < 0
void vnode_hw_uid(uint8_t uid[12]);        // the MCU's unique ID
// the node number and name kept in flash: load returns 0 when there are none
int vnode_hw_load(uint8_t *node, char name[16]);
int vnode_hw_store(uint8_t node, const char name[16]);
// console output waiting to go out (printf): up to max bytes, returns how many
int vnode_hw_con_read(char *buf, int max);
