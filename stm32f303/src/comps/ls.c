#include "ls_comp.h"
#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"
#include "stm32f3xx_hal.h"
#include "common.h"
#include "f3hw.h"
#include "ringbuf.h"
#include "vbrake.h"

extern CRC_HandleTypeDef hcrc;

HAL_COMP(ls);

//process data from LS
HAL_PIN(d_cmd);
HAL_PIN(q_cmd);
HAL_PIN(pos);
HAL_PIN(vel);
HAL_PIN(en);

// config data from LS
HAL_PIN(cmd_mode);
HAL_PIN(phase_mode);
HAL_PIN(r);
HAL_PIN(l);
HAL_PIN(psi);
HAL_PIN(cur_bw);
HAL_PIN(cur_ff);
HAL_PIN(cur_ind);
HAL_PIN(max_y);
HAL_PIN(max_cur);
HAL_PIN(dac);

// process data to LS
HAL_PIN(dc_volt);
HAL_PIN(id_fb);
HAL_PIN(iq_fb);
HAL_PIN(ud_fb);
HAL_PIN(uq_fb);

// state data to LS
HAL_PIN(hv_temp);
HAL_PIN(mot_temp);
HAL_PIN(core_temp);
HAL_PIN(fault_in);  //fault code send to f4
HAL_PIN(ignore_fault_pin);
HAL_PIN(y);
HAL_PIN(u_fb);
HAL_PIN(v_fb);
HAL_PIN(w_fb);

// misc
HAL_PIN(pwm_volt);
HAL_PIN(crc_error);
HAL_PIN(crc_ok);
HAL_PIN(timeout);
HAL_PIN(dma_pos);
HAL_PIN(idle);
HAL_PIN(fault);  //communication fault output

HAL_PIN(dma_pos2);
HAL_PIN(arr);
HAL_PIN(dma_pos_cmd);
HAL_PIN(inc);
HAL_PIN(window);

// the F4 stopped talking while the drive was enabled, and asked for it
// (flags.brake_on_loss): brake here, on the HV side, which runs from the DC
// bus (docs/emergency_stop.md): a virtual resistor (shared/vbrake.h) in
// voltage mode, frame frozen, until the current has died down
HAL_PIN(brake);     // out: braking
HAL_PIN(brake_rv);  // out: [ohm]
HAL_PIN(vbrake);    // out: to curpid0.brake: the F4's brake (flags.brake) or this one

struct ls_ctx_t {
  uint32_t timeout;
  uint32_t tx_addr;
  uint8_t send;
  volatile packet_to_hv_t packet_to_hv;
  volatile packet_from_hv_t packet_from_hv;
  int was_en;       // the last packet enabled the drive ...
  int brake_flag;   // ... and asked for a brake on loss
  int braking, brake_done;
  float brake_t;
  vbrake_t vb;
};

//TODO: move to ctx
// volatile packet_to_hv_t packet_to_hv;
// volatile packet_from_hv_t packet_from_hv;

f3_config_data_t config;
f3_state_data_t state;

static void hw_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct ls_ctx_t *ctx = (struct ls_ctx_t *)ctx_ptr;
  // struct ls_pin_ctx_t * pins = (struct ls_pin_ctx_t *)pin_ptr;

  GPIO_InitTypeDef GPIO_InitStruct;

  /* Peripheral clock enable */
  __HAL_RCC_USART3_CLK_ENABLE();

  UART_HandleTypeDef huart3;
  huart3.Instance                    = USART3;
  huart3.Init.BaudRate               = DATABAUD;
  huart3.Init.WordLength             = UART_WORDLENGTH_8B;
  huart3.Init.StopBits               = UART_STOPBITS_1;
  huart3.Init.Parity                 = UART_PARITY_NONE;
  huart3.Init.Mode                   = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling           = UART_OVERSAMPLING_8;
  huart3.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  USART3->CR3 |= USART_CR3_DMAT | USART_CR3_DMAR | USART_CR3_OVRDIS;
  HAL_UART_Init(&huart3);

  /**USART3 GPIO Configuration    
   PB10     ------> USART3_TX
   PB11     ------> USART3_RX 
   */
  GPIO_InitStruct.Pin       = GPIO_PIN_10 | GPIO_PIN_11;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  __HAL_RCC_DMA1_CLK_ENABLE();

  //TX DMA
  DMA1_Channel2->CCR &= (uint16_t)(~DMA_CCR_EN);
  DMA1_Channel2->CPAR  = (uint32_t) & (USART3->TDR);
  DMA1_Channel2->CMAR  = (uint32_t) & (ctx->packet_from_hv);
  DMA1_Channel2->CNDTR = sizeof(packet_from_hv_t);
  DMA1_Channel2->CCR   = DMA_CCR_MINC | DMA_CCR_DIR;  // | DMA_CCR_PL_0 | DMA_CCR_PL_1
  DMA1->IFCR           = DMA_IFCR_CTCIF2 | DMA_IFCR_CHTIF2 | DMA_IFCR_CGIF2;

  //RX DMA
  DMA1_Channel3->CCR &= (uint16_t)(~DMA_CCR_EN);
  DMA1_Channel3->CPAR  = (uint32_t) & (USART3->RDR);
  DMA1_Channel3->CMAR  = (uint32_t) & (ctx->packet_to_hv);
  DMA1_Channel3->CNDTR = sizeof(packet_to_hv_t);
  DMA1_Channel3->CCR   = DMA_CCR_MINC;  // | DMA_CCR_PL_0 | DMA_CCR_PL_1
  DMA1->IFCR           = DMA_IFCR_CTCIF3 | DMA_IFCR_CHTIF3 | DMA_IFCR_CGIF3;
  DMA1_Channel3->CCR |= DMA_CCR_EN;

  config.pins.r       = 0.0;
  config.pins.l       = 0.0;
  config.pins.psi     = 0.0;
  config.pins.cur_bw   = 0.0;
  config.pins.cur_ff  = 0.0;
  config.pins.cur_ind = 0.0;
  config.pins.max_y   = 0.0;
  config.pins.max_cur = 0.0;
  config.pins.dac     = 0.0;

  USART3->RTOR = 16;               // 16 bits timeout
  USART3->CR2 |= USART_CR2_RTOEN;  // timeout en
  USART3->ICR |= USART_ICR_RTOCF;  // timeout clear flag

  ctx->packet_from_hv.header.len        = (sizeof(packet_from_hv_t) - sizeof(stmbl_talk_header_t)) / 4;
  ctx->packet_from_hv.header.flags.cmd  = WRITE_CONF;
  ctx->packet_from_hv.header.slave_addr = 0;
}

static void rt_start(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct ls_ctx_t *ctx      = (struct ls_ctx_t *)ctx_ptr;
  struct ls_pin_ctx_t *pins = (struct ls_pin_ctx_t *)pin_ptr;

  ctx->timeout     = 0;
  ctx->tx_addr     = 0;
  for(int i = 0; i < 3; i++) {  // unique ID, reported to the F4 with the state
    state.pins.uid[i] = ((const uint32_t *)0x1FFFF7AC)[i];
  }
  ctx->send        = 0;
  PIN(crc_error)   = 0.0;
  PIN(crc_ok)      = 0.0;
  PIN(timeout)     = 0.0;
  PIN(idle)        = 0.0;
  PIN(dma_pos_cmd) = 4;
  PIN(inc)         = 5;
  PIN(window)      = 1;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct ls_ctx_t *ctx      = (struct ls_ctx_t *)ctx_ptr;
  struct ls_pin_ctx_t *pins = (struct ls_pin_ctx_t *)pin_ptr;

  uint32_t dma_pos = sizeof(packet_to_hv_t) - DMA1_Channel3->CNDTR;

  PIN(dma_pos2) = dma_pos;
  PIN(arr)      = PWM_RES;

  if(dma_pos > PIN(window) && dma_pos < sizeof(packet_to_hv_t) - PIN(window)) {
    if(PIN(dma_pos_cmd) < dma_pos) {
      PIN(arr) = PWM_RES - PIN(inc);
    } else if(PIN(dma_pos_cmd) > dma_pos) {
      PIN(arr) = PWM_RES + PIN(inc);
    }
  }

  uint32_t fault = 0;

  if(dma_pos == sizeof(packet_to_hv_t)) {
    // CRC covers the packet from header.slave_addr (word 1) on
    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)&(ctx->packet_to_hv) + 1, sizeof(packet_to_hv_t) / 4 - 1);
    if(ctx->packet_to_hv.header.slave_addr == 0 && ctx->packet_to_hv.header.len == (sizeof(packet_to_hv_t) - sizeof(stmbl_talk_header_t)) / 4 && crc == ctx->packet_to_hv.header.crc) {
      //
      uint8_t a = ctx->packet_to_hv.header.conf_addr;
      a         = CLAMP(a, 0, sizeof(config) / 4 - 1);  // the last valid word

      switch(ctx->packet_to_hv.header.flags.cmd) {
        case NO_CMD:
          break;
        case WRITE_CONF:
          config.data[a] = ctx->packet_to_hv.header.config.f32;  // TODO: first enable after complete update
          break;
        case READ_CONF:
          ctx->tx_addr = a;
          break;
        case DO_RESET:
          NVIC_SystemReset();
          break;
        case BOOTLOADER:
          RTC->BKP0R = 0xDEADBEEF;
          NVIC_SystemReset();
          break;
      }

      PIN(en)         = ctx->packet_to_hv.flags.enable;
      PIN(phase_mode) = ctx->packet_to_hv.flags.phase_type;
      PIN(cmd_mode)   = ctx->packet_to_hv.flags.cmd_type;
      PIN(vbrake)     = ctx->packet_to_hv.flags.brake;
      PIN(ignore_fault_pin) = ctx->packet_to_hv.flags.ignore_fault_pin;
      PIN(d_cmd)      = ctx->packet_to_hv.d_cmd;
      PIN(q_cmd)      = ctx->packet_to_hv.q_cmd;
      PIN(pos)        = ctx->packet_to_hv.pos;
      PIN(vel)        = ctx->packet_to_hv.vel;

      if(ctx->packet_to_hv.flags.buf != 0x0) {
        extern struct ringbuf rx_buf;
        rb_write(&rx_buf, (void *)&(ctx->packet_to_hv.flags.buf), 1);
      }

      PIN(r)       = config.pins.r;
      PIN(l)       = config.pins.l;
      PIN(psi)     = config.pins.psi;
      PIN(cur_bw)   = config.pins.cur_bw;
      PIN(cur_ff)  = config.pins.cur_ff;
      PIN(cur_ind) = config.pins.cur_ind;
      PIN(max_y)   = config.pins.max_y;
      PIN(max_cur) = config.pins.max_cur;
      PIN(dac)     = config.pins.dac;
      ctx->timeout    = 0;
      ctx->was_en     = ctx->packet_to_hv.flags.enable;
      ctx->brake_flag = ctx->packet_to_hv.flags.brake_on_loss;
      ctx->braking    = 0;
      ctx->brake_done = 0;
      PIN(brake)      = 0.0;
      PIN(crc_ok)
      ++;
      if(ctx->send == 0) {
        ctx->send = 1;
      }
    } else {
      PIN(crc_error)
      ++;
      fault = 3;
    }
  }else if(ctx->timeout <= 5){// if no packet and no timeout, advance pos by velovity
    PIN(pos) = PIN(pos) + PIN(vel) * period;
  }


  if(USART3->ISR & USART_ISR_RTOF) {                                    // idle line
    USART3->ICR |= USART_ICR_RTOCF | USART_ICR_FECF | USART_ICR_ORECF;  // timeout clear flag
    GPIOA->BSRR |= GPIO_PIN_10;

    PIN(idle)
    ++;
    if(dma_pos != sizeof(packet_to_hv_t)) {
      PIN(dma_pos) = dma_pos;
    }

    // reset rx DMA
    DMA1_Channel3->CCR &= (uint16_t)(~DMA_CCR_EN);
    DMA1_Channel3->CNDTR = sizeof(packet_to_hv_t);
    DMA1_Channel3->CCR |= DMA_CCR_EN;
    dma_pos = 0;
    GPIOA->BSRR |= GPIO_PIN_10 << 16;

    //ctx->send = 1;
  }

  if(ctx->send == 2) {
    ctx->send = 0;
  }
  if(ctx->send == 1 && dma_pos != 0) {
    ctx->send = 2;
    //packet_to_hv.d_cmd = -99.0;
    state.pins.u_fb      = PIN(u_fb);
    state.pins.v_fb      = PIN(v_fb);
    state.pins.w_fb      = PIN(w_fb);
    state.pins.hv_temp   = PIN(hv_temp);
    state.pins.mot_temp  = PIN(mot_temp);
    state.pins.core_temp = PIN(core_temp);
    state.pins.y         = PIN(y);
    state.pins.dc_volt   = PIN(dc_volt);
    state.pins.pwm_volt  = PIN(pwm_volt);

    // fill tx struct
    ctx->packet_from_hv.fault             = (uint8_t)PIN(fault_in);
    ctx->packet_from_hv.id_fb              = PIN(id_fb);
    ctx->packet_from_hv.iq_fb              = PIN(iq_fb);
    ctx->packet_from_hv.ud_fb              = PIN(ud_fb);
    ctx->packet_from_hv.uq_fb              = PIN(uq_fb);
    ctx->packet_from_hv.header.conf_addr  = ctx->tx_addr;
    ctx->packet_from_hv.header.config.u32 = state.u32[ctx->tx_addr++];
    ctx->tx_addr %= sizeof(state) / 4;

    extern struct ringbuf tx_buf;
    uint8_t buf[1];
    if(rb_read(&tx_buf, buf, 1)) {
      ctx->packet_from_hv.buf = buf[0];
    } else {
      ctx->packet_from_hv.buf = 0x0;
    }
    ctx->packet_from_hv.header.crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)&(ctx->packet_from_hv) + 1, sizeof(packet_from_hv_t) / 4 - 1);

    // start tx DMA
    DMA1_Channel2->CCR &= (uint16_t)(~DMA_CCR_EN);
    DMA1_Channel2->CNDTR = sizeof(packet_from_hv_t);
    DMA1_Channel2->CCR |= DMA_CCR_EN;
    //ctx->send = 0;
  }

  if(ctx->timeout > 5) {  // no F4: brake if asked, else disable the driver
    PIN(vel) = 0.0;
    if(ctx->brake_flag && ctx->was_en && !ctx->brake_done) {
      if(!ctx->braking) {
        ctx->braking = 1;
        ctx->brake_t = 0.0;
        vbrake_reset(&ctx->vb);
      }
      // below curpid's own voltage-mode clamp at max_cur and the stage's
      // ABS_MAX_CURRENT; rv only rises to hold the current, and backs off
      // before the bus reaches ABS_MAX_VOLT
      vbrake_cfg_t c = {.max_cur      = 0.9 * MIN(PIN(max_cur), ABS_MAX_CURRENT),
                        .pwm_volt     = PIN(pwm_volt),
                        .dc_volt      = PIN(dc_volt),
                        .high_dc_volt = 0.9 * ABS_MAX_VOLT,
                        .kp           = 0.1,
                        .ki           = 500.0,
                        .stop_cur     = 0.05 * PIN(max_cur),
                        .stop_time    = 0.1,
                        .l            = PIN(l),
                        .delay        = 1.5 * period,
                        .trip         = 1.3,
                        .trip_time    = 0.002};
      float ud, uq;
      vbrake_step(&ctx->vb, &c, PIN(id_fb), PIN(iq_fb), period, &ud, &uq);
      ctx->brake_t += period;
      PIN(en)       = 1.0;
      PIN(cmd_mode) = VOLT_MODE;
      PIN(d_cmd)    = ud;
      PIN(q_cmd)    = uq;
      PIN(brake)    = 1.0;
      PIN(vbrake)   = 1.0;
      PIN(brake_rv) = ctx->vb.rv;
      if(ctx->vb.stopped || ctx->vb.gave_up || ctx->brake_t > 2.0) {
        ctx->brake_done = 1;
      }
    } else {
      PIN(en)     = 0.0;
      PIN(brake)  = 0.0;
      PIN(vbrake) = 0.0;
    }
    PIN(timeout)
    ++;
    fault = 1;
  }
  ctx->timeout++;

  PIN(fault) = MAX(fault, PIN(fault_in));


  // TODO: sin = 0.5
  switch((uint16_t)PIN(phase_mode)) {
    case PHASE_90_3PH:  // 90°
      PIN(pwm_volt) = PIN(dc_volt) / M_SQRT2 * 0.95;
      break;

    case PHASE_90_4PH:  // 90°
      PIN(pwm_volt) = PIN(dc_volt) * 0.95;
      break;

    case PHASE_120_3PH:  // 120°
      PIN(pwm_volt) = PIN(dc_volt) / M_SQRT3 * 0.95;
      break;

    case PHASE_180_2PH:  // 180°
    case PHASE_180_3PH:  // 180°
      PIN(pwm_volt) = PIN(dc_volt) * 0.95;
      break;

    default:
      PIN(pwm_volt) = 0.0;
  }
}

hal_comp_t ls_comp_struct = {
    .name      = "ls",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .hw_init   = hw_init,
    .rt_start  = rt_start,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = sizeof(struct ls_ctx_t),
    .pin_count = sizeof(struct ls_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
