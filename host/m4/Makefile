# Unit tests built for the drive's CPU (Cortex-M4F, the firmware's flags) and
# run in QEMU (mps2-an386, semihosting): the ARM-only paths the host build
# can't take, e.g. shared/adc_sum.h's uadd16.
#
#   make -C host/m4 check     (needs arm-none-eabi-gcc and qemu-system-arm;
#                              skips with a note if either is missing)
ROOT  = ../..
CC    = arm-none-eabi-gcc
QEMU  = qemu-system-arm
CPU   = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -fsingle-precision-constant
CFLAGS = -O2 -std=gnu11 -Wall $(CPU) --specs=rdimon.specs -I$(ROOT)/shared
TESTS = test_adc_sum

check:
	@if ! command -v $(CC) >/dev/null || ! command -v $(QEMU) >/dev/null; then \
		echo "host/m4: $(CC) or $(QEMU) missing, skipped"; exit 0; fi; \
	$(MAKE) -s $(TESTS:%=%.elf) && for t in $(TESTS); do \
		timeout 300 $(QEMU) -machine mps2-an386 -cpu cortex-m4 -nographic \
			-semihosting-config enable=on,target=native -kernel $$t.elf | sed 's/^/m4: /' || exit 1; done

# the toolchain's default linker script, linked at 0 with the vector table first
m4.ld:
	$(CC) $(CPU) --specs=rdimon.specs -Wl,--verbose 2>/dev/null | sed -n '/^======/,/^======/p' | sed '1d;$$d' | \
		sed 's/PROVIDE (__executable_start = SEGMENT_START("text-segment", 0x8000)); \. = SEGMENT_START("text-segment", 0x8000);/PROVIDE (__executable_start = 0); . = 0; .vectors : { KEEP(*(.vectors)) }/' > $@
	@grep -q '\.vectors' $@ || (echo "m4.ld: no vector table placed"; rm -f $@; exit 1)

test_adc_sum.elf: $(ROOT)/host/tests/test_adc_sum.c $(ROOT)/shared/adc_sum.h $(ROOT)/shared/sserial_sync.h start.c m4.ld
	$(CC) $(CFLAGS) -T m4.ld -o $@ start.c $< -lm -lrdimon

clean:
	rm -f *.elf m4.ld

.PHONY: check clean
