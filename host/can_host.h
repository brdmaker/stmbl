#pragma once
#include "stmblcan_bus.h"
// host/can_host.c: a simulated CAN bus between the simulated drive and
// LinuxCNC's end (linuxcnc/stmblcan's bus code on a simulated MCP2515)
int can_host_init(int node, float vel_scale);
// ask the drive for its description (discovery): 0 ok, < 0 none
int can_host_discover(int node, char *buf, int len, void (*wait)(void *ctx, int us));
stmblcan_node_t *can_host_node(void);  // the node LinuxCNC talks to
void can_host_read(void);              // LinuxCNC's servo thread: first
void can_host_write(void);             // ... and last
void can_host_run(double us);          // bus time
void can_host_drive_attached(int on);  // unplug the drive
void can_host_stat(void);
