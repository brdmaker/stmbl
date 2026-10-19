# fbtest Arduino Uno firmware: make, make flash (PORT=/dev/ttyUSB0)
# (uno_flash.py, not avrdude: see its header)
PORT ?= /dev/ttyUSB0
MCU = atmega328p

fbtest.hex: fbtest.elf
	avr-objcopy -O ihex -R .eeprom $< $@
	avr-size $<

fbtest.elf: fbtest.c
	avr-gcc -mmcu=$(MCU) -DF_CPU=16000000UL -Os -Wall -Wextra -o $@ $<

flash: fbtest.hex
	../uno_flash.py $< $(PORT)

clean:
	rm -f fbtest.elf fbtest.hex

.PHONY: flash clean
