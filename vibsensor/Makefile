# vibsensor firmware (STM32G473CE): stmbl's HAL with the vibration sensor's
# components (docs/vibsensor.md). From the repo root: make -C vibsensor
#
#   make            build obj/vibsensor.elf / .bin / .hex, print the size
#   make BOOT=1     link behind a bootloader (0x08004000); the default
#                   (BOOT=0) links at 0x08000000 to flash alone over SWD
#   make flash      st-flash the .bin (SWD), see CLAUDE.md for the rules
#   make option_bytes  once per new chip (SWD, STM32CubeProgrammer): BOOT0
#                   pin ignored (PB8 is CAN RX), dual bank
#   make clean

ROOT   = ..
OBJDIR = obj
GEN    = $(OBJDIR)/gen
TARGET = $(OBJDIR)/vibsensor
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

# the board's components, then the shared ones
BRD_COMPS    = iis thermo
SHARED_COMPS = vib vnode
COMP_SRCS    = $(addprefix comps/,$(addsuffix .c,$(BRD_COMPS))) \
               $(addprefix $(ROOT)/shared/comps/,$(addsuffix .c,$(SHARED_COMPS)))
COMP_HDRS    = $(addprefix $(GEN)/,$(addsuffix _comp.h,$(BRD_COMPS) $(SHARED_COMPS)))

SRCS = src/main.c src/hw.c src/fdcan.c src/flash.c src/config.c src/syscalls.c src/cdc.c \
       $(ROOT)/shared/hal.c $(ROOT)/shared/commands.c $(ROOT)/shared/vibdsp.c $(ROOT)/shared/vibaddr.c \
       $(ROOT)/lib/CMSIS/Device/ST/STM32G4xx/Source/system_stm32g4xx.c \
       $(COMP_SRCS) $(GEN)/hal_tbl.c $(GEN)/conf_templates.c
ASRCS = $(ROOT)/lib/CMSIS/Device/ST/STM32G4xx/Source/gcc/startup_stm32g473xx.s

TEMPLATES = $(wildcard conf/*.txt)

# the HAL's pools, sized for a sensor (~140 pins; vib's big buffers are static)
HAL_SIZES = -DHAL_MAX_PINS=320 -DHAL_MAX_COMPS=8 -DHAL_MAX_CTX=4096

CPU      = -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16
INCLUDES = -Iinc -I$(GEN) -I$(ROOT)/shared -I$(ROOT)/lib/CMSIS5/Include -I$(ROOT)/lib/CMSIS/Device/ST/STM32G4xx/Include
CFLAGS   = $(CPU) -O2 -g -std=gnu11 -Wall -ffunction-sections -fdata-sections -fsingle-precision-constant \
           -DSTM32G473xx -DHSE_VALUE=8000000 $(HAL_SIZES) $(INCLUDES)
LDFLAGS  = $(CPU) -specs=nano.specs -specs=nosys.specs -u _printf_float -Wl,--gc-sections \
           -Wl,-Map=$(TARGET).map -T$(OBJDIR)/g473.ld -lm

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

PROGRAMMER ?= STM32_Programmer_CLI
option_bytes:
	$(PROGRAMMER) -c port=SWD -ob nSWBOOT0=0 nBOOT0=1 DBANK=1
option_bytes_show:
	$(PROGRAMMER) -c port=SWD -ob displ

clean:
	rm -rf $(OBJDIR)

.PHONY: all flash option_bytes option_bytes_show clean
.SECONDARY:
