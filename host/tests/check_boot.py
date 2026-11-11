#!/usr/bin/env python3
"""Source checks for the bootloaders and hv_update (no simulator runs them):
each uses shared/boot_logic.h (host/tests/test_boot_logic.c), DEBUGNOTES
2026-11-11.
- bootloader/src/main.c: PA13 through boot_pin_wants_dfu with the debugger
  state and many samples, not one read; clocks reset before the ROM jump.
- f3_boot/src/main.c: the erase through boot_erase_pages (blank pages
  skipped), not one HAL_FLASHEx_Erase over the whole app area.
- src/comps/hv.c: ERASE_FLASH gives up after BOOT_ERASE_TICKS, CRC_CHECK
  paced by boot_crc_send."""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")


def read(rel):
    s = open(os.path.join(ROOT, rel)).read()
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


n = fails = 0


def ok(c, name):
    global n, fails
    n += 1
    fails += not c
    print("%s %d - boot: %s" % ("ok" if c else "not ok", n, name))


bl = read("bootloader/src/main.c")
ok(re.search(r"boot_pin_wants_dfu\([^;]*DHCSR[^;]*,\s*100\s*\)", bl, re.S) is not None,
   "F4 bootloader: PA13 sampled 100 times, not under a debugger")
ok(re.search(r"=\s*!\s*GPIO_ReadInputDataBit\(GPIOA,\s*GPIO_Pin_13\)\s*;", bl) is None, "F4 bootloader: no single PA13 read")
jump = bl[: bl.find("SysMemBootJump()")]
ok("RCC_DeInit()" in jump[jump.rfind("if(requested"):], "F4 bootloader: clocks reset (PLL off) before the ROM jump")

f3 = read("f3_boot/src/main.c")
ok("boot_erase_pages(" in f3, "F3 bootloader: erase through boot_erase_pages (blank pages skipped)")
ok(re.search(r"NbPages\s*=\s*(?![\s1])", f3) is None, "F3 bootloader: one page an erase, no whole-area erase")

hv = read("src/comps/hv.c")
ok(re.search(r"timeout\s*>\s*BOOT_ERASE_TICKS", hv) is not None, "hv_update: the erase gets BOOT_ERASE_TICKS")
ok("boot_crc_send(" in hv, "hv_update: CRC_CHECK paced by boot_crc_send")
print("1..%d" % n)
sys.exit(1 if fails else 0)
