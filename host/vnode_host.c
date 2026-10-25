/*
 * The vibration sensor's bus in the host simulator (shared/vnode_hw.h): the
 * frames vnode sends wait in a list, frames for it are put in by command.
 *   vnin ID HEX      a frame from the bus (ID hex or decimal, data in hex)
 *   vnout            print the frames sent since the last vnout, one a line:
 *                    "vnout ID LEN HEX", then "vnout N frames"
 *   vnuid HEX        the unique ID (before load vnode)
 *   vnflash          what is stored as node and name
 * The console's output is the simulator's stdout, as everywhere here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "vnode_hw.h"

#define QN 512
static vibp_frame_t inq[QN], outq[QN];
static int in_r, in_w, out_n;
static uint8_t uid[12] = {0x20, 0x38, 0x47, 0x56, 0x10, 0x00, 0x51, 0x00, 0x41, 0x30, 0x34, 0x12};
static uint8_t fl_node, fl_ok;
static char fl_name[16];

int vnode_hw_recv(vibp_frame_t *f) {
  if(in_r == in_w) {
    return 0;
  }
  *f   = inq[in_r];
  in_r = (in_r + 1) % QN;
  return 1;
}

int vnode_hw_send(const vibp_frame_t *f) {
  if(out_n >= QN) {
    return -1;
  }
  outq[out_n++] = *f;
  return 0;
}

void vnode_hw_uid(uint8_t u[12]) {
  memcpy(u, uid, 12);
}

int vnode_hw_load(uint8_t *node, char name[16]) {
  if(!fl_ok) {
    return 0;
  }
  *node = fl_node;
  memcpy(name, fl_name, 16);
  return 1;
}

int vnode_hw_store(uint8_t node, const char name[16]) {
  fl_ok   = 1;
  fl_node = node;
  memcpy(fl_name, name, 16);
  return 0;
}

int vnode_hw_con_read(char *buf, int max) {
  (void)buf;
  (void)max;
  return 0;
}

static int hex(const char *s, uint8_t *d, int max) {
  int n = 0;
  while(s[0] && s[1] && n < max) {
    if(s[0] == ' ') {
      s++;
      continue;
    }
    char b[3] = {s[0], s[1], 0};
    d[n++]    = (uint8_t)strtoul(b, 0, 16);
    s += 2;
  }
  return n;
}

void vnin(char *ptr) {
  char *end;
  long id = strtol(ptr, &end, 0);
  vibp_frame_t f;
  memset(&f, 0, sizeof(f));
  f.id  = (uint16_t)id;
  f.len = (uint8_t)hex(end, f.d, 64);
  if(f.len > 8) {
    f.len = vibp_fd_len(f.len);
  }
  if((in_w + 1) % QN == in_r) {
    printf("vnin: full\n");
    return;
  }
  inq[in_w] = f;
  in_w      = (in_w + 1) % QN;
}
COMMAND("vnin", vnin, "vnin ID HEX: a frame from the vibration bus");

void vnout(char *ptr) {
  (void)ptr;
  for(int i = 0; i < out_n; i++) {
    printf("vnout 0x%03x %d ", outq[i].id, outq[i].len);
    for(int k = 0; k < outq[i].len; k++) {
      printf("%02x", outq[i].d[k]);
    }
    printf("\n");
  }
  printf("vnout %d frames\n", out_n);
  out_n = 0;
}
COMMAND("vnout", vnout, "print the frames the vibration sensor sent");

void vnuid(char *ptr) {
  uint8_t u[12];
  if(hex(ptr, u, 12) == 12) {
    memcpy(uid, u, 12);
  }
}
COMMAND("vnuid", vnuid, "vnuid HEX: the simulated sensor's unique ID (before load vnode)");

void vnflash(char *ptr) {
  (void)ptr;
  printf("vnflash %s node %d name %.16s\n", fl_ok ? "stored" : "empty", fl_node, fl_name);
}
COMMAND("vnflash", vnflash, "what the simulated sensor keeps in flash");
