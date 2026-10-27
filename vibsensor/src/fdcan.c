/*
 * FDCAN1 on the measurement bus (docs/vibsensor.md §3), and the rest of what
 * vnode needs of the board (shared/vnode_hw.h): the unique ID, the node
 * number and name in flash, the console's output.
 *
 * Bit timing from the 85 MHz kernel clock (PLL Q):
 *   arbitration 1 Mbit/s: 85 tq, sample point 80 % (seg1 67, seg2 17)
 *   data        5 Mbit/s: 17 tq, sample point 76 % (seg1 12, seg2 4),
 *               transmitter delay compensation at 13 tq
 * The G4's message RAM is laid out by the hardware (RM0440, FDCAN chapter:
 * check): 28 standard filters, 8 extended, Rx FIFO 0 and 1 of 3 elements,
 * the Tx event FIFO, 3 Tx buffers; an element is 18 words (2 header, 64
 * bytes). Frames are received in the interrupt into a ring of 64, and sent
 * from a ring of 64 (a spectrum is 40 frames) as Tx buffers come free.
 * Every frame goes out as CAN FD with bit rate switch.
 */
#include <string.h>
#include "vs.h"
#include "flash.h"
#include "vnode_hw.h"

#define MRAM ((volatile uint32_t *)SRAMCAN_BASE)  // FDCAN1's part
#define MR_RXF0 (0x0B0 / 4)
#define MR_TXB (0x278 / 4)
#define MR_WORDS (0x350 / 4)
#define ELEM 18

#define RXQ 64
#define TXQ 64
static vibp_frame_t rxq[RXQ], txq[TXQ];
static volatile uint32_t rx_w, rx_r, tx_w, tx_r;
volatile uint32_t fdcan_rx_lost, fdcan_busoff;

static const uint8_t dlc_len[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
static uint32_t len_dlc(uint8_t len) {
  for(uint32_t d = 0; d < 16; d++) {
    if(dlc_len[d] >= len) {
      return d;
    }
  }
  return 15;
}

void fdcan_init(void) {
  RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_FDCANSEL) | RCC_CCIPR_FDCANSEL_0;  // PLL Q, 85 MHz
  RCC->APB1ENR1 |= RCC_APB1ENR1_FDCANEN;
  (void)RCC->APB1ENR1;
  // PB8 RX, PB9 TX (AF9), PB13 transceiver standby low
  GPIOB->MODER   = (GPIOB->MODER & ~((3u << 16) | (3u << 18) | (3u << 26))) | (2u << 16) | (2u << 18) | (1u << 26);
  GPIOB->AFR[1]  = (GPIOB->AFR[1] & ~((15u << 0) | (15u << 4))) | (9u << 0) | (9u << 4);
  GPIOB->OSPEEDR |= (3u << 16) | (3u << 18);
  GPIOB->BSRR = 1u << (13 + 16);

  FDCAN1->CCCR |= FDCAN_CCCR_INIT;
  while(!(FDCAN1->CCCR & FDCAN_CCCR_INIT)) {
  }
  FDCAN1->CCCR |= FDCAN_CCCR_CCE;
  FDCAN1->CCCR |= FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE;
  FDCAN1->NBTP = (16u << FDCAN_NBTP_NSJW_Pos) | (0u << FDCAN_NBTP_NBRP_Pos) | (66u << FDCAN_NBTP_NTSEG1_Pos) |
                 (16u << FDCAN_NBTP_NTSEG2_Pos);
  FDCAN1->DBTP = FDCAN_DBTP_TDC | (0u << FDCAN_DBTP_DBRP_Pos) | (11u << FDCAN_DBTP_DTSEG1_Pos) |
                 (3u << FDCAN_DBTP_DTSEG2_Pos) | (3u << FDCAN_DBTP_DSJW_Pos);
  FDCAN1->TDCR = 13u << FDCAN_TDCR_TDCO_Pos;
  // no filters: every standard frame into Rx FIFO 0 (vnode sorts them); no
  // extended or remote frames
  FDCAN1->RXGFC = (0u << FDCAN_RXGFC_ANFS_Pos) | (2u << FDCAN_RXGFC_ANFE_Pos) | FDCAN_RXGFC_RRFS | FDCAN_RXGFC_RRFE;
  FDCAN1->TXBC  = 0;  // Tx FIFO mode
  for(int i = 0; i < MR_WORDS; i++) {
    MRAM[i] = 0;
  }
  FDCAN1->IE  = FDCAN_IE_RF0NE | FDCAN_IE_TCE;
  FDCAN1->ILS = 0;                  // all on line 0
  FDCAN1->ILE = FDCAN_ILE_EINT0;
  FDCAN1->TXBTIE = 7;               // transmission complete of each buffer
  NVIC_SetPriority(FDCAN1_IT0_IRQn, 3);
  NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
  FDCAN1->CCCR &= ~FDCAN_CCCR_INIT;
}

// from the Tx ring into free Tx buffers
static void tx_fill(void) {
  while(tx_r != tx_w && !(FDCAN1->TXFQS & FDCAN_TXFQS_TFQF)) {
    const vibp_frame_t *f = &txq[tx_r % TXQ];
    uint32_t idx          = (FDCAN1->TXFQS & FDCAN_TXFQS_TFQPI) >> FDCAN_TXFQS_TFQPI_Pos;
    volatile uint32_t *e  = MRAM + MR_TXB + idx * ELEM;
    e[0]                  = (uint32_t)(f->id & 0x7FF) << 18;
    e[1]                  = (len_dlc(f->len) << 16) | (1u << 21) | (1u << 20);  // FDF, BRS
    uint32_t w[16];
    memset(w, 0, sizeof(w));
    memcpy(w, f->d, f->len);
    for(int i = 0; i < 16; i++) {
      e[2 + i] = w[i];
    }
    FDCAN1->TXBAR = 1u << idx;
    tx_r++;
  }
}

void FDCAN1_IT0_IRQHandler(void) {
  uint32_t ir = FDCAN1->IR;
  FDCAN1->IR  = ir;
  while(FDCAN1->RXF0S & FDCAN_RXF0S_F0FL) {
    uint32_t gi                = (FDCAN1->RXF0S & FDCAN_RXF0S_F0GI) >> FDCAN_RXF0S_F0GI_Pos;
    const volatile uint32_t *e = MRAM + MR_RXF0 + gi * ELEM;
    if(rx_w - rx_r < RXQ) {
      vibp_frame_t *f = &rxq[rx_w % RXQ];
      f->id           = (e[0] >> 18) & 0x7FF;
      f->len          = dlc_len[(e[1] >> 16) & 15];
      uint32_t w[16];
      for(int i = 0; i < 16; i++) {
        w[i] = e[2 + i];
      }
      memcpy(f->d, w, 64);
      rx_w++;
    } else {
      fdcan_rx_lost++;
    }
    FDCAN1->RXF0A = gi;
  }
  tx_fill();
}

void fdcan_poll(void) {
  if(FDCAN1->PSR & FDCAN_PSR_BO) {  // bus off: start again (it waits 128 x 11 recessive bits)
    fdcan_busoff++;
    FDCAN1->CCCR &= ~FDCAN_CCCR_INIT;
  }
  NVIC_DisableIRQ(FDCAN1_IT0_IRQn);
  tx_fill();
  NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
}

// ---- shared/vnode_hw.h ----
int vnode_hw_recv(vibp_frame_t *f) {
  if(rx_r == rx_w) {
    return 0;
  }
  *f = rxq[rx_r % RXQ];
  rx_r++;
  return 1;
}

int vnode_hw_send(const vibp_frame_t *f) {
  if(tx_w - tx_r >= TXQ) {
    return -1;
  }
  txq[tx_w % TXQ] = *f;
  tx_w++;
  NVIC_DisableIRQ(FDCAN1_IT0_IRQn);
  tx_fill();
  NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
  return 0;
}

void vnode_hw_uid(uint8_t uid[12]) {
  memcpy(uid, (const void *)UID_BASE, 12);
}

typedef struct {
  uint32_t magic;
  uint8_t node, pad[3];
  char name[16];
} vs_id_t;  // 24 bytes: three double words
#define ID_MAGIC 0x44495356u  // "VSID"

int vnode_hw_load(uint8_t *node, char name[16]) {
  const vs_id_t *id = (const vs_id_t *)FLASH_ID;
  if(id->magic != ID_MAGIC) {
    return 0;
  }
  *node = id->node;
  memcpy(name, id->name, 16);
  return 1;
}

int vnode_hw_store(uint8_t node, const char name[16]) {
  vs_id_t id;
  memset(&id, 0, sizeof(id));
  id.magic = ID_MAGIC;
  id.node  = node;
  memcpy(id.name, name, 16);
  if(flash_erase_page(FLASH_ID)) {
    return -1;
  }
  return flash_program(FLASH_ID, &id, sizeof(id));
}

int vnode_hw_con_read(char *buf, int max) {
  extern int con_out_read(char *buf, int max);  // src/syscalls.c
  return con_out_read(buf, max);
}
