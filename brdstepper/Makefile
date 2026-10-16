# brdstepper firmware (STM32G473CE): stmbl's HAL and shared components with
# brdstepper's drivers. From the repo root: make -C brdstepper
#
#   make            build obj/brdstepper.elf / .bin / .hex, print the size
#   make BOOT=1     link behind the bootloader (0x08004000); the default
#                   (BOOT=0) links at 0x08000000 to flash alone over SWD
#   make flash      st-flash the .bin (SWD), see CLAUDE.md for the rules
#   make clean

ROOT   = ..
OBJDIR = obj
GEN    = $(OBJDIR)/gen
TARGET = $(OBJDIR)/brdstepper
PYTHON ?= python3

PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

BOOT ?= 0
ifeq ($(BOOT),1)
APP_ORIGIN = 0x08004000
APP_LENGTH = 240K
else
APP_ORIGIN = 0x08000000
APP_LENGTH = 256K
endif

# the board's components, then the shared ones the templates load
BRD_COMPS    = hv io adc mag
SHARED_COMPS = reslimit rev fb_switch vel pid fault sim iit pe scope fwup stp_ttc term
COMP_SRCS    = $(addprefix comps/,$(addsuffix .c,$(BRD_COMPS))) \
               $(addprefix $(ROOT)/shared/comps/,$(addsuffix .c,$(SHARED_COMPS))) \
               $(ROOT)/src/comps/conf.c
COMP_HDRS    = $(addprefix $(GEN)/,$(addsuffix _comp.h,$(BRD_COMPS) $(SHARED_COMPS) conf))

SRCS = src/main.c src/hw.c src/flash.c src/config.c src/fwup_g4.c src/syscalls.c src/cdc.c \
       $(ROOT)/shared/hal.c $(ROOT)/shared/commands.c $(ROOT)/shared/config.c $(ROOT)/shared/angle.c \
       $(ROOT)/shared/ringbuf.c $(ROOT)/shared/sscon.c $(ROOT)/shared/fwstage.c $(ROOT)/shared/bip_loop.c \
       $(ROOT)/lib/CMSIS/Device/ST/STM32G4xx/Source/system_stm32g4xx.c \
       $(COMP_SRCS) $(GEN)/hal_tbl.c $(GEN)/conf_templates.c
ASRCS = $(ROOT)/lib/CMSIS/Device/ST/STM32G4xx/Source/gcc/startup_stm32g473xx.s

TEMPLATES = $(wildcard $(ROOT)/conf/template/*.txt) $(wildcard conf/*.txt)

CPU      = -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16
INCLUDES = -Iinc -I$(GEN) -I$(ROOT)/shared -I$(ROOT)/lib/CMSIS5/Include -I$(ROOT)/lib/CMSIS/Device/ST/STM32G4xx/Include
CFLAGS   = $(CPU) -O2 -g -std=gnu11 -Wall -ffunction-sections -fdata-sections -fsingle-precision-constant \
           -DSTM32G473xx -DHSE_VALUE=8000000 $(INCLUDES)
LDFLAGS  = $(CPU) -specs=nano.specs -specs=nosys.specs -u _printf_float -Wl,--gc-sections \
           -Wl,-Map=$(TARGET).map -T$(OBJDIR)/g473.ld -lm

# objects by path (comps/hv.c and ../src/comps/hv.c must not meet)
OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(subst $(ROOT)/,root/,$(SRCS))) \
       $(patsubst %.s,$(OBJDIR)/%.o,$(subst $(ROOT)/,root/,$(ASRCS)))

all: $(TARGET).bin $(TARGET).hex
	@$(SIZE) $(TARGET).elf

$(OBJDIR)/g473.ld: g473.ld Makefile | $(OBJDIR)
	@printf "APP_ORIGIN = $(APP_ORIGIN);\nAPP_LENGTH = $(APP_LENGTH);\n" > $@
	@cat g473.ld >> $@

$(TARGET).elf: $(OBJS) $(OBJDIR)/g473.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(OBJDIR)/root/%.o: $(ROOT)/%.c $(COMP_HDRS) $(GEN)/commandslist.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/root/%.o: $(ROOT)/%.s
	@mkdir -p $(dir $@)
	$(CC) $(CPU) -c $< -o $@
$(OBJDIR)/%.o: %.c $(COMP_HDRS) $(GEN)/commandslist.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(GEN)/%_comp.h: comps/%.c | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_comp_h.py $@ $<
$(GEN)/%_comp.h: $(ROOT)/shared/comps/%.c | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_comp_h.py $@ $<
$(GEN)/conf_comp.h: $(ROOT)/src/comps/conf.c | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_comp_h.py $@ $<
$(GEN)/hal_tbl.c: $(COMP_SRCS) | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_hal_tbl.py $@ $(COMP_SRCS)
$(GEN)/conf_templates.c: $(TEMPLATES) | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_config.py $@ $(TEMPLATES)
$(GEN)/commandslist.h: $(SRCS) | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_cmd.py $@ $(filter-out $(GEN)/%,$(SRCS))

$(OBJDIR) $(GEN):
	@mkdir -p $@

# SWD, the whole image at 0x08000000 (BOOT=0)
flash: $(TARGET).bin
	st-flash --reset write $(TARGET).bin $(APP_ORIGIN)

clean:
	rm -rf $(OBJDIR)

.PHONY: all flash clean
