/*
 * Setup wizard: wiring help.
 *
 * The connector tables are the ones in docs/src/Getting Started.adoc
 * ("Pinouts"): the 8P8C (RJ45) command and feedback connectors with the
 * T568B wire colours, a column per signal type. The functions return
 * malloc'd fixed-width text for the UI (SU_MSG_WIRING).
 */
#pragma once

// the feedback connector for one column ("Encoder", "Resolver", "1Vpp",
// "UVW", "Mitsubishi", "Sanyo-Denki", "Yaskawa", "Omron", "RS485", ...);
// NULL for an unknown column
char *su_wiring_feedback(const char *column);

// the command connector: "Smart Serial", "Step/Dir", "Quadrature"
char *su_wiring_command(const char *column);

// HV supply, motor phases, logic supply (brake: a line pointing to the
// brake's wiring below)
char *su_wiring_power(int brake, double bus);

// the holding brake on output out (0..2, DIO0..2): directly when it is
// 24 V DC and takes at most 1 A, else through a relay
char *su_wiring_brake(int out, int volt24, double current);

// the motor temperature sensor (type: conf0.mot_temp_type) on the HV board
char *su_wiring_temp(int type);
// the sensor on a feedback port's pin 7 (port 0: FB0, 1: FB1), the jumper to open
char *su_wiring_temp_fb(int type, int port);
