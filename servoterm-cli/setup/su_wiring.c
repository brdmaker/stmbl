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

char *su_wiring_brake(int out, int volt24, double current) {
  char *s = malloc(2048);
  int o   = snprintf(s, 2048,
                     "Holding brake, on DIO%d (the 6-way socket below the 24 V connector, DIO0 nearest\n"
                     "the top; each output switches to GND, with a 24 V pin beside it; 24 V, 1 A,\n"
                     "2 A absolute). The output is on (brake released) while the drive is enabled.\n",
                     out);
  if(volt24 && current > 0.0 && current <= 1.0) {
    snprintf(s + o, 2048 - o,
             "  - The brake coil between DIO%d and the 24 V pin beside it.\n"
             "  - A flyback diode across the coil (cathode to +24 V), or the output's\n"
             "    switching spike reaches the drive's logic supply.\n",
             out);
  } else {
    snprintf(s + o, 2048 - o,
             "  - The brake %s: switch it with a relay.\n"
             "    The relay's coil (24 V, under 1 A) between DIO%d and the 24 V pin beside it,\n"
             "    with a flyback diode across that coil; the relay's contact switches the\n"
             "    brake's own supply, with a suppressor (diode or varistor) across the brake.\n",
             !volt24 ? "isn't 24 V DC" : current > 1.0 ? "takes more than the output's 1 A" : "current is unknown",
             out);
  }
  return s;
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
    o += snprintf(out + o, 2048 - o, "  - Holding brake: below.\n");
  }
  snprintf(out + o, 2048 - o,
           "  - Keep the HV and LV sides isolated: USB ties the PC's GND to the LV board;\n"
           "    never let it reach the HV side.\n");
  return out;
}

// The sensor on a feedback port's pin 7 instead (port 0: FB0, 1: FB1;
// docs/motor_temp_lv.md): the port's supply jumper opened, the sensor
// against pin 8 (GND), measured on the LV side.
char *su_wiring_temp_fb(int type, int port) {
  char *s = malloc(1536);
  snprintf(s, 1536,
           "Motor temperature sensor (%s) on %s, the RJ45 %s:\n"
           "  pin 7 (brown stripe)  sensor\n"
           "  pin 8 (brown)         sensor, GND\n"
           "Either way round (a KTY84: its band, the cathode, to pin 8). Before that, open the\n"
           "port's supply jumper %s on the LV board: a 3-pad solder jumper, its centre pad\n"
           "joined to the +5 V pad by a trace. Cut that trace (pin 7 must read 10 kohm to +5 V,\n"
           "the pull-up, not 0), leave the +12 V pad open. Pin 7 then no longer powers an\n"
           "encoder: %s. See docs/motor_temp_lv.md.\n"
           "The sensor is then on the LV side, with the feedback: its insulation inside the motor\n"
           "must be rated for that, as the feedback's (SEW CMP: the KTY84 is in the feedback\n"
           "cable, brown / white, next to the resolver: it goes to the same plug).\n",
           type == 1 ? "KTY84-130" : type == 2 ? "PT1000" : type == 3 ? "PTC" : type == 4 ? "NTC" : "thermal switch",
           port ? "FB1" : "FB0", port ? "second feedback port" : "feedback port (with the resolver)",
           port ? "JP5" : "JP2",
           port ? "nothing else may use FB1" : "only a resolver works on FB0 then (it needs no supply)");
  return s;
}

char *su_wiring_temp(int type) {
  char *s = malloc(1024);
  snprintf(s, 1024,
           "Motor temperature sensor (%s): its two wires to the HV board's 2-pin motor\n"
           "temperature connector (3.5 mm). Either way round: the drive measures its resistance.\n"
           "That input is on the HV side (the DC bus' potential, read by the F3): treat the\n"
           "sensor's wires as HV wiring, never let them touch the LV side or the feedback cable.\n"
           "(On SEW CMP motors the KTY84 is in the feedback cable, brown / white: take that pair\n"
           "out of the feedback connector to the HV board. In the cable it then runs at the HV\n"
           "side's potential next to the LV feedback pairs: only the wires' insulation separates\n"
           "them, so keep the pair's ends insulated and apart from the RJ45.)\n",
           type == 1 ? "KTY84-130" : type == 2 ? "PT1000" : type == 3 ? "PTC" : type == 4 ? "NTC" : "thermal switch");
  return s;
}
