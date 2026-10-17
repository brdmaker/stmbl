/*
 * stmblcan: stmbl drives over CAN from LinuxCNC on a Raspberry Pi, with
 * MCP2515 controllers driven directly over SPI from the servo thread
 * (docs/can_support.md). Polled, no interrupts, no SocketCAN in the loop.
 *
 *   loadrt stmblcan spi=/dev/spidev0.0,/dev/spidev0.1,/dev/spidev0.2 nodes=1,2:3,4:5,6
 *   addf stmblcan.read  servo-thread     (first: the drives' feedback)
 *   ... motion, pid ...
 *   addf stmblcan.write servo-thread     (last: commands and SYNC)
 *
 * Parameters: spi (one spidev per bus), nodes (per bus, ':' between
 * buses, up to 2 a bus), spi_hz (10 MHz, the MCP2515's maximum), bitrate
 * (1000000), osc (16000000), vel_scale (100: vel_cmd / vel_fb full scale),
 * timeout (10 periods).
 *
 * Pins, for bus B and node N (stmblcan.B.N.*): pos-cmd vel-cmd enable
 * index-enable out-0..3 con-cmd (in); pos-fb vel-fb fault index in-0..3
 * con-fb current power temp vbus present missed (out). Per bus
 * (stmblcan.B.*): tx-late rx-overflow foreign tec rec, ok.
 * con-cmd / con-fb are the console's 10-bit field: link them to
 * sserialterm for a pty per drive, as over smart-serial.
 *
 * The SPI devices: the Pi's overlay binds the MCP2515s to the kernel's
 * mcp251x driver (SocketCAN, for candump); spi_takeover.sh hands them to
 * spidev before LinuxCNC starts and back afterwards.
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "stmblcan_bus.h"

MODULE_AUTHOR("stmbl");
MODULE_DESCRIPTION("stmbl drives over CAN, MCP2515s on SPI from the servo thread");
MODULE_LICENSE("GPL");

static char *spi = "";
RTAPI_MP_STRING(spi, "spidev devices, one per bus, comma separated");
static char *nodes = "";
RTAPI_MP_STRING(nodes, "node IDs per bus: 1,2:3,4:5,6");
static int spi_hz = 10000000;
RTAPI_MP_INT(spi_hz, "SPI clock [Hz]");
static int bitrate = 1000000;
RTAPI_MP_INT(bitrate, "CAN bit rate");
static int osc = 16000000;
RTAPI_MP_INT(osc, "MCP2515 crystal [Hz]");
static char *vel_scale = "100";
RTAPI_MP_STRING(vel_scale, "full scale of vel-cmd / vel-fb");
static int timeout = 10;
RTAPI_MP_INT(timeout, "periods without feedback before a node is not present");

#define MAXBUS 4

typedef struct {
  hal_float_t *pos_cmd, *vel_cmd, *pos_fb, *vel_fb, *current, *power, *temp, *vbus;
  hal_bit_t *enable, *index_enable, *out[4], *fault, *index, *in[4], *present;
  hal_u32_t *con_cmd, *con_fb, *missed;
} node_pins_t;

typedef struct {
  hal_u32_t *tx_late, *rx_overflow, *foreign, *tec, *rec;
  hal_bit_t *ok;
  node_pins_t node[STMBLCAN_NODES];
} bus_pins_t;

typedef struct {
  int fd;
  uint32_t hz;
} spidev_t;

static int comp_id, nbus;
static stmblcan_bus_t *bus;
static bus_pins_t *pins;
static spidev_t dev[MAXBUS];
static int bus_ok[MAXBUS];

// mcp_xfer_fn on spidev: one ioctl, chip select released between segments
static int spidev_xfer(void *ctx, mcp_seg_t *segs, int n) {
  spidev_t *d = ctx;
  struct spi_ioc_transfer tr[8];
  if(n > 8) {
    return -1;
  }
  memset(tr, 0, sizeof(tr));
  for(int i = 0; i < n; i++) {
    tr[i].tx_buf        = (unsigned long)segs[i].tx;
    tr[i].rx_buf        = (unsigned long)segs[i].rx;
    tr[i].len           = segs[i].len;
    tr[i].speed_hz      = d->hz;
    tr[i].bits_per_word = 8;
    tr[i].cs_change     = i < n - 1;  // release the chip select between instructions
  }
  return ioctl(d->fd, SPI_IOC_MESSAGE(n), tr) < 0 ? -1 : 0;
}

static void read_all(void *arg, long period) {
  (void)arg;
  (void)period;
  for(int b = 0; b < nbus; b++) {
    if(!bus_ok[b]) {
      continue;
    }
    stmblcan_bus_t *s = &bus[b];
    bus_pins_t *p     = &pins[b];
    if(stmblcan_bus_read(s) < 0) {
      bus_ok[b] = 0;
      *p->ok    = 0;
      continue;
    }
    for(int i = 0; i < s->nnodes; i++) {
      stmblcan_node_t *n = &s->node[i];
      node_pins_t *q     = &p->node[i];
      *q->pos_fb         = n->fb.pos;
      *q->vel_fb         = n->fb.vel;
      *q->fault          = n->fb.fault || !n->present;
      *q->index          = n->fb.index;
      for(int k = 0; k < 4; k++) {
        *q->in[k] = (n->fb.in >> k) & 1;
      }
      *q->con_fb  = n->fb.con;
      *q->current = n->slots[CANP_SLOT_CURRENT];
      *q->power   = n->slots[CANP_SLOT_POWER];
      *q->temp    = n->slots[CANP_SLOT_TEMP];
      *q->vbus    = n->slots[CANP_SLOT_VBUS];
      *q->present = n->present;
      *q->missed  = n->missed;
    }
    *p->tx_late     = s->mcp.tx_late;
    *p->rx_overflow = s->mcp.rx_overflow;
    *p->foreign     = s->foreign;
    *p->tec         = s->mcp.tec;
    *p->rec         = s->mcp.rec;
  }
}

static void write_all(void *arg, long period) {
  (void)arg;
  (void)period;
  for(int b = 0; b < nbus; b++) {
    if(!bus_ok[b]) {
      continue;
    }
    stmblcan_bus_t *s = &bus[b];
    bus_pins_t *p     = &pins[b];
    for(int i = 0; i < s->nnodes; i++) {
      stmblcan_node_t *n = &s->node[i];
      node_pins_t *q     = &p->node[i];
      n->cmd.pos          = (float)*q->pos_cmd;
      n->cmd.vel          = (float)*q->vel_cmd;
      n->cmd.enable       = *q->enable;
      n->cmd.index_enable = *q->index_enable;
      n->cmd.out          = 0;
      for(int k = 0; k < 4; k++) {
        n->cmd.out |= (uint8_t)((*q->out[k] ? 1 : 0) << k);
      }
      n->cmd.con = (uint16_t)(*q->con_cmd & 0x3ff);
    }
    if(stmblcan_bus_write(s) < 0) {
      bus_ok[b] = 0;
      *p->ok    = 0;
    }
  }
}

static int node_pins(node_pins_t *q, int b, int n) {
  int r = 0;
#define FP(dir, f, name) r |= hal_pin_float_newf(dir, &q->f, comp_id, "stmblcan.%d.%d." name, b, n)
#define BP(dir, f, name) r |= hal_pin_bit_newf(dir, &q->f, comp_id, "stmblcan.%d.%d." name, b, n)
#define UP(dir, f, name) r |= hal_pin_u32_newf(dir, &q->f, comp_id, "stmblcan.%d.%d." name, b, n)
  FP(HAL_IN, pos_cmd, "pos-cmd");
  FP(HAL_IN, vel_cmd, "vel-cmd");
  BP(HAL_IN, enable, "enable");
  BP(HAL_IN, index_enable, "index-enable");
  UP(HAL_IN, con_cmd, "con-cmd");
  FP(HAL_OUT, pos_fb, "pos-fb");
  FP(HAL_OUT, vel_fb, "vel-fb");
  FP(HAL_OUT, current, "current");
  FP(HAL_OUT, power, "power");
  FP(HAL_OUT, temp, "temp");
  FP(HAL_OUT, vbus, "vbus");
  BP(HAL_OUT, fault, "fault");
  BP(HAL_OUT, index, "index");
  BP(HAL_OUT, present, "present");
  UP(HAL_OUT, con_fb, "con-fb");
  UP(HAL_OUT, missed, "missed");
  for(int k = 0; k < 4; k++) {
    r |= hal_pin_bit_newf(HAL_IN, &q->out[k], comp_id, "stmblcan.%d.%d.out-%d", b, n, k);
    r |= hal_pin_bit_newf(HAL_OUT, &q->in[k], comp_id, "stmblcan.%d.%d.in-%d", b, n, k);
  }
#undef FP
#undef BP
#undef UP
  return r;
}

int rtapi_app_main(void) {
  comp_id = hal_init("stmblcan");
  if(comp_id < 0) {
    return comp_id;
  }
  bus  = calloc(MAXBUS, sizeof(stmblcan_bus_t));
  pins = hal_malloc(MAXBUS * sizeof(bus_pins_t));
  if(!bus || !pins) {
    hal_exit(comp_id);
    return -ENOMEM;
  }
  float vs = (float)atof(vel_scale ? vel_scale : "100");
  char devs[1024], nds[256];
  snprintf(devs, sizeof(devs), "%s", spi ? spi : "");
  snprintf(nds, sizeof(nds), "%s", nodes ? nodes : "");
  char *save_d, *save_n;
  char *d   = strtok_r(devs, ",", &save_d);
  char *grp = strtok_r(nds, ":", &save_n);
  for(; d && grp && nbus < MAXBUS; d = strtok_r(NULL, ",", &save_d), grp = strtok_r(NULL, ":", &save_n)) {
    int b             = nbus;
    stmblcan_bus_t *s = &bus[b];
    spidev_t *sd      = &dev[b];
    sd->hz            = (uint32_t)spi_hz;
    sd->fd            = open(d, O_RDWR);
    if(sd->fd < 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: %s: %s (spi_takeover.sh run?)\n", d, strerror(errno));
      hal_exit(comp_id);
      return -ENODEV;
    }
    uint8_t mode = SPI_MODE_0, bits = 8;
    uint32_t hz  = (uint32_t)spi_hz;
    ioctl(sd->fd, SPI_IOC_WR_MODE, &mode);
    ioctl(sd->fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(sd->fd, SPI_IOC_WR_MAX_SPEED_HZ, &hz);
    s->mcp.xfer = spidev_xfer;
    s->mcp.ctx  = sd;
    s->timeout  = (uint32_t)timeout;
    char *save_g;
    for(char *t = strtok_r(grp, ",", &save_g); t && s->nnodes < STMBLCAN_NODES; t = strtok_r(NULL, ",", &save_g)) {
      int id = atoi(t);
      if(id < 1 || id > CANP_NODE_MAX) {
        rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: node %s: 1..127\n", t);
        hal_exit(comp_id);
        return -EINVAL;
      }
      s->node[s->nnodes++] = (stmblcan_node_t){.node = id, .vel_scale = vs};
    }
    int r = stmblcan_bus_init(s, (uint32_t)osc, (uint32_t)bitrate);
    if(r < 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: %s: MCP2515 init failed (%d): wiring, chip select, SPI clock?\n", d, r);
      hal_exit(comp_id);
      return -ENODEV;
    }
    bus_ok[b]    = 1;
    bus_pins_t *p = &pins[b];
    int e        = hal_pin_u32_newf(HAL_OUT, &p->tx_late, comp_id, "stmblcan.%d.tx-late", b);
    e |= hal_pin_u32_newf(HAL_OUT, &p->rx_overflow, comp_id, "stmblcan.%d.rx-overflow", b);
    e |= hal_pin_u32_newf(HAL_OUT, &p->foreign, comp_id, "stmblcan.%d.foreign", b);
    e |= hal_pin_u32_newf(HAL_OUT, &p->tec, comp_id, "stmblcan.%d.tec", b);
    e |= hal_pin_u32_newf(HAL_OUT, &p->rec, comp_id, "stmblcan.%d.rec", b);
    e |= hal_pin_bit_newf(HAL_OUT, &p->ok, comp_id, "stmblcan.%d.ok", b);
    for(int i = 0; i < s->nnodes; i++) {
      e |= node_pins(&p->node[i], b, s->node[i].node);
    }
    if(e) {
      hal_exit(comp_id);
      return -EINVAL;
    }
    *p->ok = 1;
    rtapi_print_msg(RTAPI_MSG_INFO, "stmblcan: bus %d on %s, %d nodes\n", b, d, s->nnodes);
    nbus++;
  }
  if(nbus < 1) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: spi=<spidev>[,...] nodes=<id>[,<id>][:...] needed\n");
    hal_exit(comp_id);
    return -EINVAL;
  }
  if(hal_export_funct("stmblcan.read", read_all, NULL, 1, 0, comp_id) ||
     hal_export_funct("stmblcan.write", write_all, NULL, 1, 0, comp_id)) {
    hal_exit(comp_id);
    return -EINVAL;
  }
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  for(int b = 0; b < nbus; b++) {
    close(dev[b].fd);
  }
  hal_exit(comp_id);
}
