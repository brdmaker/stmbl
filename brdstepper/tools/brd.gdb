# brdstepper bring-up over SWD, before the board has a console
# (docs/brdstepper_hw.md, section 9). With OpenOCD and an ST-Link:
#   openocd -f interface/stlink.cfg -f target/stm32g4x.cfg
#   arm-none-eabi-gdb -x tools/brd.gdb obj/brdstepper.elf
#
# Each command halts the core for a moment. While the drive is enabled the
# timers keep running with the last duty, and the DRV8876's own trip
# (VREF) limits the current. Prefer these with the drive disabled, or the
# motor free and the supply current-limited.

target extended-remote :3333

define brd
  printf "vbus %.2f V  ntc %.3f V  hwid %.3f V  variant %d  periods %u  hal_state %d\n", brd_adc.vbus, brd_adc.ntc_v, brd_adc.hwid_v, brd_variant, brd_adc_count, hal.hal_state
  printf "IPROPI A drive %.4f brake %.4f   B drive %.4f brake %.4f  [A]\n", brd_adc.ia_drive, brd_adc.ia_brake, brd_adc.ib_drive, brd_adc.ib_brake
end
document brd
The board's samples: supply, NTC, ID divider, the four IPROPI readings.
end

define pin
  printf "%s%d.%s = %f\n", "$arg0", $arg1, "$arg2", pin_inst_by_name("$arg0", $arg1, "$arg2")->source->value
end
document pin
pin COMP INSTANCE PIN: a HAL pin's value, e.g. "pin hv 0 iq_fb", "pin mag 0 pos".
end

define hal
  call hal_parse("$arg0 = $arg1")
end
document hal
hal SINK SOURCE_OR_VALUE: a config line, e.g. "hal rev0.in 6.283" (one turn),
"hal fault0.en 1" (enable without the enable input), "hal stp_ttc0.detent 0".
end

define enc
  call hw_as5047_read(0xFFFF)
  p/x hw_as5047_read(0xC000)
  printf "angle %d of 16384, error flag %d\n", $ & 0x3fff, ($ >> 14) & 1
end
document enc
One AS5047P angle read: ANGLECOM, then a NOP to clock it out.
end
