/*
 * Process data between the simulated drive and whatever plays the LinuxCNC
 * side (the stmbl_sim LinuxCNC module, or host commands). It carries the same
 * fields as the smart-serial packets in src/comps/sserial.c.
 */
#pragma once
#include <stdint.h>

typedef struct {
  // to the drive (LinuxCNC -> sserial0)
  float pos_cmd;
  float vel_cmd;
  float scale;
  uint8_t out[4];
  uint8_t enable;
  uint8_t index_enable;
  uint32_t seq;  // bumped by the writer on every update

  // from the drive (sserial0 -> LinuxCNC)
  float pos_fb;
  float vel_fb;
  float current;
  float power;  // the drive's input power [W]
  uint16_t con_cmd, con_fb;  // the console fields (shared/sscon.h)
  uint8_t in[4];
  uint8_t fault;
  uint8_t index_enable_fb;  // drops to 0 when the drive found the index
} sim_sserial_io_t;

extern sim_sserial_io_t sim_sserial_io;  // sserial port 0: the CMD port
// sserial port p (shared/sserial_port.h): 0 CMD, 1 enc0, 2 enc1, 3 enc0 on USART6; 0 if none
sim_sserial_io_t *sim_sserial_port(int p);
