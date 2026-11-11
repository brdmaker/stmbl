/*
 * The F3's real stm32f303/src/comps/io.c on the host, with the real
 * shared/hal.c and a register stand-in (host/f3sim). The F3 fixes of
 * 2026-11-10/11 (DEBUGNOTES), run instead of read:
 * - A1: the software overcurrent trip on every phase, either sign;
 * - A3: rt_stop (any hal stop) turns the bridge off: TIM8 MOE and HV_EN;
 * - A4: a stalled ADC DMA stops the hal (MISC_ERROR), bridge off;
 * - A5: a break with the PVD latched is HV_UNDERVOLT_ERROR (22), else 16.
 * Prints TAP.
 */
#include "../../shared/hal.c"
#include "io_comp.h"  // generated from the F3's io.c (gen_f3/)
#include "stm32f3xx_hal.h"
#include "common.h"
#include "f3hw.h"

uint32_t hal_get_systick_value() {
  return 0;
}
uint32_t hal_get_systick_reload() {
  return 1;
}
uint32_t hal_get_systick_freq() {
  return 1;
}
volatile int32_t cmd_active = -1;
volatile uint64_t systime;
uint32_t call_cmd(char *s) {
  (void)s;
  return 0;
}

extern hal_comp_t io_comp_struct;
extern volatile uint32_t adc_12_buf[6], adc_34_buf[6];
const hal_comp_t *comps[] = {&io_comp_struct};
const pin_t pins[64];
const uint32_t comp_count = 1;
const uint32_t pin_count  = 64;

static int n, fails;
static void ok(int c, const char *name) {
  n++;
  fails += !c;
  printf("%s %d - f3_io: %s\n", c ? "ok" : "not ok", n, name);
}

// (io writes its fault pin before the break check: a break shows a tick later)
// the current measurement: after the offset calibration a phase reads
// k * (raw0 - raw) A (io.c's AMP, SHUNT_GAIN 16)
#define RAW0 2048
static double k_amp(void) {
  return AREF / 4096.0 / 16.0 / (SHUNT * SHUNT_PULLUP) * (SHUNT_PULLUP + SHUNT_SERIE);
}
static void set_currents(double iu, double iv, double iw) {
  uint32_t u = (uint32_t)(RAW0 - iu / k_amp() + 0.5);
  uint32_t v = (uint32_t)(RAW0 - iv / k_amp() + 0.5);
  uint32_t w = (uint32_t)(RAW0 - iw / k_amp() + 0.5);
  for(int i = 0; i < 5; i++) {
    adc_12_buf[i] = (u << 16) | w;  // the 12 pair: U high, W low
    adc_34_buf[i] = (2000u << 16) | v;  // 34: V low (high: hv temp, unused here)
  }
  adc_12_buf[5] = 0;
  adc_34_buf[5] = (uint32_t)(300.0 / ((AREF / 4096.0) / VDIVDOWN * (VDIVUP + VDIVDOWN))) << 16;  // udc 300 V
}

static struct io_pin_ctx_t *io;
static int stall;  // the ADC DMA never completes
static void tick(void) {
  if(!stall) {
    DMA1->ISR = DMA_ISR_TCIF1;
    DMA2->ISR = DMA_ISR_TCIF5;
  } else {
    DMA1->ISR = DMA2->ISR = 0;
  }
  hal_run_rt();
}
static int moe(void) {
  return (TIM8->BDTR & TIM_BDTR_MOE) != 0;
}
static int hv_en(void) {
  return (GPIOA->ODR & HV_EN_PIN) != 0;
}

static void boot(void) {
  hal_init(1.0 / 15000.0, 0.0);
  hal_set_debug_level(2);
  TIM8->BDTR = 0;
  PWR->CSR   = 0;
  GPIOA->ODR = 0;
  GPIOB->IDR = HV_FAULT_PIN;  // the gate driver's fault input idles high (no fault)
  stall      = 0;
  load_comp(comp_by_name("io"));
  io               = (struct io_pin_ctx_t *)hal.comp_insts[0].pin_insts;
  io->rt_prio.value = 1.0;
  hal_start();
  set_currents(0, 0, 0);
  for(int i = 0; i < 210; i++) {  // the offset calibration (200 ticks)
    tick();
  }
}
static int run_enabled(int ticks) {
  io->hv_en.value = 1.0;
  for(int i = 0; i < ticks; i++) {
    tick();
  }
  return (int)io->fault.value;
}

int main(void) {
  setvbuf(stdout, 0, _IONBF, 0);  // a hang (the old ADC wait) still shows how far it got
  boot();
  ok(io->fault.value == 0 && fabsf(io->iu.value) < 0.05 && fabsf(io->iv.value) < 0.05, "offsets calibrated, no fault");
  set_currents(5, -2, -3);
  ok(run_enabled(5) == 0 && moe() && hv_en(), "enabled: MOE set, HV_EN high, 5 A no fault");
  ok(fabsf(io->iu.value - 5) < 0.1 && fabsf(io->iv.value + 2) < 0.1, "currents read back (5, -2 A)");

  // A1: a peak above ABS_MAX_CURRENT (30 A) on every phase and sign
  const char *ph[3] = {"U", "V", "W"};
  for(int p = 0; p < 3; p++) {
    for(int s = -1; s <= 1; s += 2) {
      boot();
      double i[3] = {0, 0, 0};
      i[p]        = 31.0 * s;
      set_currents(i[0], i[1], i[2]);
      char name[64];
      snprintf(name, sizeof(name), "A1: %c%s 31 A: HV_OVERCURRENT_PEAK", s > 0 ? '+' : '-', ph[p]);
      ok(run_enabled(2) == HV_OVERCURRENT_PEAK, name);
    }
  }

  // A3: any hal stop turns the bridge off
  boot();
  run_enabled(5);
  ok(moe() && hv_en(), "A3: running");
  hal_stop();
  ok(!moe() && !hv_en(), "A3: hal stopped: MOE cleared, HV_EN low");
  hal_start();
  run_enabled(3);
  ok(moe() && hv_en(), "A3: restarted and enabled: MOE set again (rt_stop forgot the enable)");

  // A4: the ADC DMA stalls
  boot();
  run_enabled(5);
  stall = 1;
  tick();
  ok(hal.rt_state == RT_STOP && hal.hal_state == MISC_ERROR, "A4: a stalled ADC: the hal stops with MISC_ERROR");
  ok(!moe() && !hv_en(), "A4: and the bridge is off");

  // A5: a hardware break (MOE cleared) with and without the PVD
  boot();
  run_enabled(5);
  TIM8->BDTR &= ~TIM_BDTR_MOE;  // a comparator broke
  ok(run_enabled(2) == HV_OVERCURRENT_HW, "A5: a break without the PVD: HV_OVERCURRENT_HW (16)");
  boot();
  run_enabled(5);
  PWR->CSR = PWR_CSR_PVDO;  // 3.3 V below the PVD
  tick();
  PWR->CSR = 0;  // it recovers 100 mV above, as the break cleared MOE
  TIM8->BDTR &= ~TIM_BDTR_MOE;
  ok(run_enabled(2) == HV_UNDERVOLT_ERROR, "A5: a break after a PVD dip: HV_UNDERVOLT_ERROR (22)");
  io->hv_en.value = 0;
  tick();
  run_enabled(1);
  TIM8->BDTR &= ~TIM_BDTR_MOE;
  ok(run_enabled(2) == HV_OVERCURRENT_HW, "A5: the PVD latch clears on disable");

  printf("1..%d\n", n);
  return fails != 0;
}
