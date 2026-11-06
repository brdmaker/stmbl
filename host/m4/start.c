// minimal Cortex-M4 start for QEMU mps2-an386 + newlib rdimon (semihosting): host/m4
extern void _start(void);
__attribute__((naked, noreturn)) static void reset(void) {
  __asm__ volatile(
      "ldr r0, =0xE000ED88\n"  // CPACR: CP10/CP11 full access (the FPU)
      "ldr r1, [r0]\n"
      "orr r1, r1, #(0xF << 20)\n"
      "str r1, [r0]\n"
      "dsb\n isb\n"
      "b _start\n");
}
__attribute__((section(".vectors"), used)) void *const vectors[2] = {(void *)0x20400000, (void *)reset};
