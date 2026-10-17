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
 * Parameters: spi (one spidev per bus), nodes (stmbl drives per bus, ':'
 * between buses, up to 2 a bus), desc (a description file per bus, ','
 * between buses, empty for none: devices without our firmware, and
 * stmbl drives pinned down), discover (1: ask each node in nodes for its
 * description at load; a file entry for the same node must match),
 * spi_hz (10 MHz, the MCP2515's maximum), bitrate (1000000), osc
 * (16000000), vel_scale (100, for drives not discovered), timeout (10
 * periods). A bus whose period does not fit the MCP2515 (3 frames out
 * with SYNC, 2 in) is refused at load.
 *
 * A described device's fields become pins stmblcan.B.<device>.<field>
 * (out for its rx frames, in for its tx frames), plus .present.
 *
 * Pins, for bus B and node N (stmblcan.B.N.*): pos-cmd vel-cmd enable
 * out-0..3 con-cmd (in); index-enable (io: LinuxCNC sets it, stmblcan
 * clears it once the drive found the index, as hm2); pos-fb vel-fb fault
 * index in-0..3 con-fb current power temp vbus present missed vel-clipped
 * (out). Per bus
 * (stmblcan.B.*): tx-late rx-overflow foreign tec rec, ok.
 * con-cmd / con-fb are the console's 10-bit field: link them to
 * sserialterm for a pty per drive, as over smart-serial.
 *
 * The SPI devices: the Pi's overlay binds the MCP2515s to the kernel's
 * mcp251x driver (SocketCAN, for candump); spi_takeover.sh hands them to
 * spidev before LinuxCNC starts and back afterwards.
 *
 * spi=sim (one bus): no Pi, the simulated drive's simulated MCP2515
 * instead (host/lcnc: loadrt stmbl_sim can=1 first). The same code from
 * the SPI instructions up, in LinuxCNC's servo thread; addf stmbl-sim.0
 * after stmblcan.write, so the bus carries the frames in the period, as
 * on the wire. hwtest/burnin-sim: make test-can.
 */
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include <errno.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
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
static char *desc = "";
RTAPI_MP_STRING(desc, "description files, one per bus, comma separated (empty: none)");
static int discover = 0;
RTAPI_MP_INT(discover, "1: ask the drives in nodes for their description at load");

#define MAXBUS 4

typedef struct {
  hal_float_t *pos_cmd, *vel_cmd, *pos_fb, *vel_fb, *current, *power, *temp, *vbus;
  hal_bit_t *enable, *index_enable, *out[4], *fault, *index, *in[4], *present;
  hal_u32_t *con_cmd, *con_fb, *missed;
  hal_u32_t *vel_clipped;  // periods with |vel-cmd| over vel_scale: raise the drive's can0.vel_scale
} node_pins_t;

typedef struct {
  hal_float_t *f[CANDESC_FRAMES][CANDESC_FIELDS];  // numeric fields
  hal_bit_t *b[CANDESC_FRAMES][CANDESC_FIELDS];    // bit fields
  hal_bit_t *present;
} dev_pins_t;

typedef struct {
  hal_u32_t *tx_late, *rx_overflow, *foreign, *tec, *rec;
  hal_bit_t *ok;
  node_pins_t node[STMBLCAN_NODES];
  dev_pins_t dev[STMBLCAN_DEVS];
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

// spi=sim: the simulated chip's SPI in stmbl_sim.so (host/lcnc)
static int find_sim(struct dl_phdr_info *info, size_t size, void *data) {
  (void)size;
  if(info->dlpi_name && strstr(info->dlpi_name, "stmbl_sim.so")) {
    snprintf(data, 512, "%s", info->dlpi_name);
    return 1;
  }
  return 0;
}

static void *sim_symbol(const char *name) {
  void *f = dlsym(RTLD_DEFAULT, name);  // rtapi_app may load modules RTLD_LOCAL:
  if(f) {
    return f;
  }
  char path[512] = "";  // then ask stmbl_sim.so itself
  dl_iterate_phdr(find_sim, path);
  void *h = path[0] ? dlopen(path, RTLD_NOW | RTLD_NOLOAD) : 0;
  if(!h) {
    return 0;
  }
  f = dlsym(h, name);
  dlclose(h);
  return f;
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
      if(n->index_found) {
        *q->index_enable = 0;
      }
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
    for(int i = 0; i < s->ndevs; i++) {
      stmblcan_dev_t *v = &s->dev[i];
      for(int f = 0; f < v->desc.nframes; f++) {
        if(v->desc.frame[f].dir != CD_RX) {
          continue;
        }
        for(int k = 0; k < v->desc.frame[f].nfields; k++) {
          if(v->desc.frame[f].field[k].type == CD_BIT) {
            *p->dev[i].b[f][k] = v->val[f][k] != 0.0f;
          } else {
            *p->dev[i].f[f][k] = v->val[f][k];
          }
        }
      }
      *p->dev[i].present = v->present;
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
      if(n->cmd.vel > n->vel_scale || n->cmd.vel < -n->vel_scale) {
        *q->vel_clipped += 1;  // int16 on the wire, full scale vel_scale: clipped
      }
      n->cmd.enable       = *q->enable;
      n->cmd.index_enable = *q->index_enable;
      n->cmd.out          = 0;
      for(int k = 0; k < 4; k++) {
        n->cmd.out |= (uint8_t)((*q->out[k] ? 1 : 0) << k);
      }
      n->cmd.con = (uint16_t)(*q->con_cmd & 0x3ff);
    }
    for(int i = 0; i < s->ndevs; i++) {
      stmblcan_dev_t *v = &s->dev[i];
      for(int f = 0; f < v->desc.nframes; f++) {
        if(v->desc.frame[f].dir != CD_TX) {
          continue;
        }
        for(int k = 0; k < v->desc.frame[f].nfields; k++) {
          v->val[f][k] = v->desc.frame[f].field[k].type == CD_BIT ? (*p->dev[i].b[f][k] ? 1.0f : 0.0f)
                                                                   : (float)*p->dev[i].f[f][k];
        }
      }
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
  BP(HAL_IO, index_enable, "index-enable");
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
  UP(HAL_OUT, vel_clipped, "vel-clipped");
  for(int k = 0; k < 4; k++) {
    r |= hal_pin_bit_newf(HAL_IN, &q->out[k], comp_id, "stmblcan.%d.%d.out-%d", b, n, k);
    r |= hal_pin_bit_newf(HAL_OUT, &q->in[k], comp_id, "stmblcan.%d.%d.in-%d", b, n, k);
  }
#undef FP
#undef BP
#undef UP
  return r;
}

static int dev_pins(dev_pins_t *q, const candesc_t *d, int b) {
  int r = 0;
  for(int f = 0; f < d->nframes; f++) {
    const candesc_frame_t *fr = &d->frame[f];
    hal_pin_dir_t dir         = fr->dir == CD_RX ? HAL_OUT : HAL_IN;
    for(int k = 0; k < fr->nfields; k++) {
      if(fr->field[k].type == CD_BIT) {
        r |= hal_pin_bit_newf(dir, &q->b[f][k], comp_id, "stmblcan.%d.%s.%s", b, d->name, fr->field[k].name);
      } else {
        r |= hal_pin_float_newf(dir, &q->f[f][k], comp_id, "stmblcan.%d.%s.%s", b, d->name, fr->field[k].name);
      }
    }
  }
  r |= hal_pin_bit_newf(HAL_OUT, &q->present, comp_id, "stmblcan.%d.%s.present", b, d->name);
  return r;
}

static void (*sim_wait)(void *ctx, int us);

static void wait_us(void *ctx, int us) {
  if(sim_wait) {  // spi=sim: the simulated drive and bus run instead
    sim_wait(ctx, us);
    return;
  }
  (void)ctx;
  usleep((useconds_t)us);
}

// the devices of a description file (several, each from its "device" line)
static int load_desc(stmblcan_bus_t *s, const char *path) {
  FILE *fp = fopen(path, "r");
  if(!fp) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: %s: %s\n", path, strerror(errno));
    return -1;
  }
  static char text[8192];
  size_t n = fread(text, 1, sizeof(text) - 1, fp);
  fclose(fp);
  text[n] = 0;
  static candesc_t all[STMBLCAN_NODES + STMBLCAN_DEVS];
  static char err[96];
  int count;
  int l = candesc_parse_all(text, all, STMBLCAN_NODES + STMBLCAN_DEVS, &count, err, sizeof(err));
  if(l) {
    rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: %s:%d: %s\n", path, l, err);
    return -1;
  }
  for(int k = 0; k < count; k++) {
    candesc_t *d = &all[k];
    if(d->stmbl && !d->node) {
      rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: %s: device %s needs a node line\n", path, d->name);
      return -1;
    }
    // a drive listed in nodes and described here: the file pins it down
    int dup = -1;
    for(int i = 0; i < s->nnodes; i++) {
      dup = d->stmbl && s->node[i].node == d->node ? i : dup;
    }
    if(dup >= 0) {
      s->node[dup].vel_scale = d->vel_scale;
    } else if(stmblcan_bus_add(s, d)) {
      rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: %s: too many devices on one bus\n", path);
      return -1;
    }
  }
  return 0;
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
  static char devs[1024], nds[256];
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
    sd->fd            = -1;
    if(!strcmp(d, "sim")) {
      mcp_xfer_fn x = (mcp_xfer_fn)sim_symbol("stmbl_sim_can_xfer");
      sim_wait      = (void (*)(void *, int))sim_symbol("stmbl_sim_can_wait");
      if(!x || !sim_wait || b > 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: spi=sim: %s\n",
                        b > 0 ? "one bus only" : "no stmbl_sim (loadrt stmbl_sim can=1 first)");
        hal_exit(comp_id);
        return -ENODEV;
      }
      s->mcp.xfer = x;
      s->mcp.ctx  = 0;
    } else {
      sd->fd = open(d, O_RDWR);
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
    }
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
    if(discover) {
      for(int i = 0; i < s->nnodes; i++) {
        static char text[160], err[96];
        static candesc_t found;
        int dr = stmblcan_bus_discover(s, (uint32_t)osc, (uint32_t)bitrate, s->node[i].node, text, sizeof(text),
                                       wait_us, 0);
        if(dr == -1) {
          rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: %s: no MCP2515\n", d);
          hal_exit(comp_id);
          return -ENODEV;
        }
        if(dr == -2 || candesc_parse(&found, text, err, sizeof(err)) || !found.stmbl) {
          rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: bus %d node %d: no stmbl description (%s)\n", b,
                          s->node[i].node, dr == -2 ? "no answer" : "not a stmbl drive");
          hal_exit(comp_id);
          return -ENODEV;
        }
        s->node[i].vel_scale = found.vel_scale;
        rtapi_print_msg(RTAPI_MSG_INFO, "stmblcan: bus %d node %d: %s, vel_scale %g\n", b, s->node[i].node,
                        found.name, (double)found.vel_scale);
      }
    }
    static char files[1024];
    snprintf(files, sizeof(files), "%s", desc ? desc : "");
    char *save_f, *file = strtok_r(files, ",", &save_f);
    for(int k = 0; k < b && file; k++) {
      file = strtok_r(NULL, ",", &save_f);
    }
    if(file && *file) {
      float before[STMBLCAN_NODES];
      for(int i = 0; i < s->nnodes; i++) {
        before[i] = s->node[i].vel_scale;
      }
      if(load_desc(s, file)) {
        hal_exit(comp_id);
        return -EINVAL;
      }
      for(int i = 0; i < s->nnodes && discover; i++) {
        if(s->node[i].vel_scale != before[i]) {
          rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: bus %d node %d: the file says vel_scale %g, the drive %g\n", b,
                          s->node[i].node, (double)s->node[i].vel_scale, (double)before[i]);
          hal_exit(comp_id);
          return -EINVAL;
        }
      }
    }
    char berr[128];
    if(stmblcan_bus_check(s, berr, sizeof(berr))) {
      rtapi_print_msg(RTAPI_MSG_ERR, "stmblcan: bus %d: %s\n", b, berr);
      hal_exit(comp_id);
      return -EINVAL;
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
    for(int i = 0; i < s->ndevs; i++) {
      e |= dev_pins(&p->dev[i], &s->dev[i].desc, b);
    }
    if(e) {
      hal_exit(comp_id);
      return -EINVAL;
    }
    *p->ok = 1;
    rtapi_print_msg(RTAPI_MSG_INFO, "stmblcan: bus %d on %s, %d drives, %d devices\n", b, d, s->nnodes, s->ndevs);
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
    if(dev[b].fd >= 0) {
      close(dev[b].fd);
    }
  }
  hal_exit(comp_id);
}
