/*
 * The configuration interface the setup wizard (servoterm-cli/setup) is
 * built against: the template names it links, the conf0 pins it sets, the
 * pins and states of the identification components it runs (idpmsm, idm,
 * ids, enc_fb's indexprint), fault0's brake_time / brake_release, the motor
 * temperature sensor (conf0.mot_temp_*, mtemp0's sensor / err / tripped), and the
 * console's getf (a pin in full precision). The drive prints it in "about"
 * ("setup api N"); the wizard refuses a drive with another number.
 * Bump it when any of these change in a way the wizard has to follow.
 */
#pragma once
#define STMBL_SETUP_API 2  // 2: the temperature sensor (mtemp0)
