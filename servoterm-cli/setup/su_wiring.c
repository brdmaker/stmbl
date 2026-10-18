#include "su_wiring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// docs/src/Getting Started.adoc, "Pinouts"
static const char *colour[8] = {"Orange Stripe", "Orange", "Green Stripe", "Blue",
                                "Blue Stripe",   "Green",  "Brown Stripe", "Brown"};

typedef struct {
  const char *name;
  const char *pin[8];
} column_t;

static const column_t feedback[] = {
    {"Resolver", {"Sin+", "Sin-", "Cos+", "Ref-", "Ref+", "Cos-", "AIN", "GND"}},
    {"Encoder", {"A+", "A-", "B+", "Z-", "Z+", "B-", "VCC", "GND"}},
    {"1Vpp", {"Sin+", "Sin-", "Cos+", "", "", "Cos-", "VCC", "GND"}},
    {"UVW", {"U+", "U-", "V+", "W-", "W+", "V-", "VCC", "GND"}},
    {"Mitsubishi", {"", "", "", "2", "1", "", "VCC", "GND"}},
    {"Sanyo-Denki", {"", "", "", "Blue", "Brown", "", "Red", "Black"}},
    {"Yaskawa", {"", "", "", "6", "5", "", "1", "2"}},
    {"Omron", {"", "", "", "4", "7", "", "6", "3"}},
    // serial protocols
    {"RS485", {"", "", "", "B", "A", "", "VCC", "GND"}},
    {"RS422", {"A", "B", "", "Z", "Y", "", "VCC", "GND"}},
};

static const column_t command[] = {
    {"Smart Serial", {"RX+", "RX-", "TX+", "", "", "TX-", "", ""}},
    {"Step/Dir", {"Step+", "Step-", "Dir+", "Err-", "Err+", "Dir-", "Enbl+", "Enbl-"}},
    {"Quadrature", {"A+", "A-", "B+", "", "", "B-", "", ""}},
};

static char *table(const column_t *cols, int n, const char *name, const char *what) {
  const column_t *c = NULL;
  for(int i = 0; i < n; i++) {
    if(strcmp(cols[i].name, name) == 0) {
      c = &cols[i];
    }
  }
  if(!c) {
    return NULL;
  }
  char *out = malloc(1024);
  int o     = snprintf(out, 1024, "%s connector (8P8C / RJ45, T568B colours), %s:\n  Pin  Colour          %s\n",
                       what, c->name, c->name);
  for(int i = 0; i < 8; i++) {
    o += snprintf(out + o, 1024 - o, "  %d    %-15s %s\n", i + 1, colour[i], c->pin[i][0] ? c->pin[i] : "-");
  }
  return out;
}

char *su_wiring_feedback(const char *column) {
  return table(feedback, sizeof(feedback) / sizeof(feedback[0]), column, "Feedback 0 (motor feedback)");
}

char *su_wiring_command(const char *column) {
  return table(command, sizeof(command) / sizeof(command[0]), column, "Command");
}

char *su_wiring_power(int brake, double bus) {
  char *out = malloc(2048);
  int o     = snprintf(out, 2048,
                       "Power (HV board connectors, 5.08 mm; LV board 3.5 mm):\n"
                       "  - Logic supply: 24 V (15..25 V) to the LV board; its green LED lights.\n"
                       "  - HV supply: the DC bus (%.0f V here; 24..380 V, 400 V absolute) to HV power +/-.\n"
                       "  - Motor: U, V, W to the HV motor connector as labelled; the motor's PE to\n"
                       "    protective earth. The phase order doesn't matter: the wizard measures it.\n",
                       bus);
  if(brake) {
    o += snprintf(out + o, 2048 - o,
                  "  - Holding brake: between DIO0 (nearest the top of the 6-way socket below the\n"
                  "    24 V connector; it switches to GND) and the 24 V pin beside it. Add a\n"
                  "    flyback diode across the brake coil. Outputs: 24 V, 1 A (2 A absolute).\n");
  }
  snprintf(out + o, 2048 - o,
           "  - Keep the HV and LV sides isolated: USB ties the PC's GND to the LV board;\n"
           "    never let it reach the HV side.\n");
  return out;
}
