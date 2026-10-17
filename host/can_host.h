#pragma once
#include "stmblcan_bus.h"
// host/can_host.c: a simulated CAN bus between the simulated drive and
// LinuxCNC's end (linuxcnc/stmblcan's bus code on a simulated MCP2515)
int can_host_init(int node, float vel_scale);
stmblcan_node_t *can_host_node(void);  // the node LinuxCNC talks to
void can_host_read(void);              // LinuxCNC's servo thread: first
void can_host_write(void);             // ... and last
void can_host_run(double us);          // bus time
void can_host_drive_attached(int on);  // unplug the drive
void can_host_stat(void);
