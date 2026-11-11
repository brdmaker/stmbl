#!/usr/bin/env python3
"""Source checks for safety wiring that no simulator runs (DEBUGNOTES
2026-11-10, from comparing with the stmbl_freak fork):

1. Every F3 comparator routed to a TIM8 break input (COMPx OUTSEL in
   stm32f303/src/main.c) has that break enabled in stm32f303/src/tim.c.
   Phase W's COMP1 goes to BKIN2, which was disabled: a phase W
   overcurrent never cut the PWM in hardware.
2. The F4's io (src/comps/hw/io4.c) has an rt_stop that drives the brake
   output PE4 low, so a stopped hal grips the holding brake (the host
   stand-in host/hw/io.c is tested in tests/ebrake/hal_stop_brake.test).
3. (A3, 2026-11-11) The F3 turns its bridge off when its rt stops: io's
   rt_stop clears TIM8's MOE and HV_EN, and the PWM interrupt does the same
   on every tick while the rt is stopped (whatever stopped it). shared/hal.c
   runs rt_stop on every stop path (tests/test_hal_stop.c).
4. (A4, 2026-11-11) The F3's io waits for the ADC DMA with a limit
   (shared/f3_safety.h, tests/test_f3_safety.c), and main.c starts the
   independent watchdog and feeds it only through f3_wdog_may_feed.
"""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")


def read(rel):
    with open(os.path.join(ROOT, rel)) as f:
        return f.read()


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


# RM0316 COMPx_CSR OUTSEL (bits 13:10) -> TIM8 break input, for the
# comparators the F3 uses (COMP1, COMP2, COMP4: 0011 = TIM8_BKIN,
# 0100 = TIM8_BKIN2, 0101 = TIM1_BKIN2 + TIM8_BKIN2)
OUTSEL_BREAK = {3: "BKIN", 4: "BKIN2", 5: "BKIN2"}

n = 0
fails = 0


def ok(c, name, detail=""):
    global n, fails
    n += 1
    fails += not c
    print("%s %d - safety_wiring: %s" % ("ok" if c else "not ok", n, name))
    if not c and detail:
        print("#   " + detail)


main = strip_comments(read("stm32f303/src/main.c"))
tim = strip_comments(read("stm32f303/src/tim.c"))

routed = {}
for m in re.finditer(r"COMP(\d)->CSR\s*=\s*([^;]+);", main):
    comp, expr = m.group(1), m.group(2)
    outsel = 0
    for b in re.findall(r"COMP%s_CSR_COMP%sOUTSEL_(\d)" % (comp, comp), expr):
        outsel |= 1 << int(b)
    brk = OUTSEL_BREAK.get(outsel)
    if brk:
        routed.setdefault(brk, []).append("COMP" + comp)

ok(sorted(routed) == ["BKIN", "BKIN2"], "the three phase comparators feed TIM8 BKIN and BKIN2",
   "routed: %r" % routed)


def field(name):
    m = re.search(r"sBreakDeadTimeConfig\.%s\s*=\s*(\w+)\s*;" % name, tim)
    return m.group(1) if m else None


state = {"BKIN": field("BreakState"), "BKIN2": field("Break2State")}
need = {"BKIN": "TIM_BREAK_ENABLE", "BKIN2": "TIM_BREAK2_ENABLE"}
for brk, comps in sorted(routed.items()):
    ok(state[brk] == need[brk], "TIM8 %s (%s) is enabled" % (brk, ", ".join(comps)),
       "tim.c sets it to %s" % state[brk])
# the same glitch filter on both, so a phase trips at the same pulse width
ok(field("Break2Filter") == field("BreakFilter"), "BKIN2 filtered as BKIN",
   "BreakFilter %s, Break2Filter %s" % (field("BreakFilter"), field("Break2Filter")))

io4 = strip_comments(read("src/comps/hw/io4.c"))
m = re.search(r"\.rt_stop\s*=\s*(\w+)", io4)
fn = m.group(1) if m else "0"
ok(fn != "0", "io4 has an rt_stop")
body = ""
if fn != "0":
    b = re.search(r"static void %s\s*\([^)]*\)\s*\{(.*?)\n\}" % fn, io4, re.S)
    body = b.group(1) if b else ""
ok(re.search(r"GPIO_ResetBits\(\s*GPIOE\s*,\s*GPIO_Pin_4\s*\)", body) is not None,
   "io4's rt_stop drives PE4 (DIO0, the brake release) low")
ok("TIM9->CCR1 = 0" in body and "TIM9->CCR2 = 0" in body, "io4's rt_stop stops the out1/out2 PWM")

# ---- the F3 (A3, A4) ----
f3io = strip_comments(read("stm32f303/src/comps/io.c"))
f3main = strip_comments(read("stm32f303/src/main.c"))


def body_of(src, fn):
    b = re.search(r"\b%s\s*\([^)]*\)\s*\{(.*?)\n\}" % fn, src, re.S)
    return b.group(1) if b else ""


m = re.search(r"\.rt_stop\s*=\s*(\w+)", f3io)
fn = m.group(1) if m else "0"
ok(fn != "0", "F3 io has an rt_stop")
stop = body_of(f3io, fn) if fn != "0" else ""
bo = body_of(f3io, "bridge_off")
ok("bridge_off()" in stop or "TIM_BDTR_MOE" in stop, "F3 io's rt_stop turns the bridge off")
ok("enabled" in stop, "F3 io's rt_stop forgets it was enabled (a restart re-arms MOE)")
ok(re.search(r"BDTR\s*&=\s*~\s*TIM_BDTR_MOE", bo) is not None, "bridge_off clears TIM8 MOE")
ok(re.search(r"HV_EN_PIN\s*,\s*GPIO_PIN_RESET", bo) is not None, "bridge_off drops HV_EN")
isr = body_of(f3main, "TIM8_UP_IRQHandler")
ok(re.search(r"rt_state\s*==\s*RT_STOP[^;]*\)\s*\{?\s*bridge_off\(\)", isr, re.S) is not None,
   "F3 PWM interrupt: bridge off on every tick while the rt is stopped")

rt = body_of(f3io, "rt_func")
ok(re.search(r"while\s*\(\s*!\s*\(\s*DMA[12]->ISR", rt) is None, "F3 io: no unbounded ADC DMA wait")
ok("f3_wait_flags(" in rt and "hal_stop()" in rt, "F3 io: bounded wait, stops the hal when it gives up")
ok(re.search(r"IWDG->KR\s*=\s*0xCCCC", f3main) is not None, "F3 starts its independent watchdog")
feeds = re.findall(r"IWDG->KR\s*=\s*0xAAAA", f3main)
ok(len(feeds) == 2 and "f3_wdog_may_feed(" in f3main,
   "F3 feeds it only at the start and through f3_wdog_may_feed")

# ---- A5: PVD and lockup break TIM8, CSS on, its NMI turns the bridge off ----
ok(re.search(r"PWR_CR_PLS_LEV6[^;]*PWR_CR_PVDE|PWR_CR_PVDE[^;]*PWR_CR_PLS_LEV6", f3main) is not None,
   "F3: PVD on at level 6 (~2.7 V)")
ok(re.search(r"SYSCFG->CFGR2\s*\|=[^;]*PVD_LOCK", f3main) is not None, "F3: the PVD breaks TIM8 (PVD_LOCK)")
ok(re.search(r"SYSCFG->CFGR2\s*\|=[^;]*LOCKUP_LOCK", f3main) is not None, "F3: a core lockup breaks TIM8")
ok(re.search(r"RCC->CR\s*\|=\s*RCC_CR_CSSON", f3main) is not None, "F3: clock security system on")
it = strip_comments(read("stm32f303/src/stm32f3xx_it.c"))
nmi = body_of(it, "NMI_Handler")
ok("RCC_CIR_CSSC" in nmi and "bridge_off()" in nmi, "F3 NMI: CSS flag cleared, bridge off")
ok(re.search(r"pvd\s*\?\s*HV_UNDERVOLT_ERROR\s*:\s*HV_OVERCURRENT_HW", f3io) is not None,
   "F3 io: a break with the PVD latched is HV_UNDERVOLT_ERROR")

print("1..%d" % n)
sys.exit(1 if fails else 0)
