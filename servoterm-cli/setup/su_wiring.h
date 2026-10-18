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

// HV supply, motor phases, holding brake, logic supply
char *su_wiring_power(int brake, double bus);
