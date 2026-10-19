// fbtest: line driver / meter for testing an STMBL feedback port with an
// Arduino Uno (ATmega328P, 16 MHz). A0..A5 (PC0..PC5) are wired to the RJ45
// pins, the board is powered from pins 7 (VCC) and 8 (GND).
//
// Serial 115200 8N1, one command per line, one answer line per command
// ("err ..." on a bad command). Numbers are decimal unless noted.
//
//   i                    identify: "fbtest <ver> pins=6"
//   o <ddr> <port>       set DDRC / PORTC bits 0..5 (hex): ddr 1 = driven,
//                        port = level; ddr 0 + port 1 = pull-up, 0 = hi-Z
//   d                    digital read: "d <pinc hex>"
//   a [n]                analog: "a <mV0> .. <mV5> vcc=<mV>", each the mean
//                        of n (default 16) samples against AVcc; vcc from the
//                        1.1 V bandgap
//   q <us> <steps> <s0> <s1> <s2> <s3>
//                        quadrature: step through the four PORTC states
//                        (hex) <steps> times (negative: backwards), <us>
//                        microseconds per step (>= 4), starting from the
//                        state after the current one; DDRC must be set
//                        before. "q <state index>"
//   c <bit> <ms>         count edges on PCx for <ms>: "c <edges>"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "1"
#define MASK 0x3f

static void uart_init(void) {
  UCSR0A = _BV(U2X0);
  UBRR0  = 16;  // 115200 at 16 MHz with U2X (2.1 %, the same as optiboot)
  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

static void putch(char c) {
  while(!(UCSR0A & _BV(UDRE0)))
    ;
  UDR0 = c;
}

static void puts_(const char *s) {
  while(*s)
    putch(*s++);
}

static void putl(long v) {
  char b[12];
  ltoa(v, b, 10);
  puts_(b);
}

static void puthex(unsigned v) {
  char b[6];
  utoa(v, b, 16);
  puts_(b);
}

static uint16_t adc_read(uint8_t mux) {
  ADMUX = mux;
  ADCSRA |= _BV(ADSC);
  while(ADCSRA & _BV(ADSC))
    ;
  return ADC;
}

// AVcc in mV from the 1.1 V bandgap (typ. 1.1 V +-10 % per part)
static long vcc_mv(void) {
  adc_read(_BV(REFS0) | 0x0e);
  _delay_ms(2);  // bandgap settles after the mux change
  uint32_t s = 0;
  for(uint8_t i = 0; i < 16; i++)
    s += adc_read(_BV(REFS0) | 0x0e);
  return 1100L * 1024L * 16L / (long)s;
}

static void cmd_analog(uint16_t n) {
  long vcc = vcc_mv();
  puts_("a");
  for(uint8_t ch = 0; ch < 6; ch++) {
    adc_read(_BV(REFS0) | ch);  // discard the first sample after the mux change
    uint32_t s = 0;
    for(uint16_t i = 0; i < n; i++)
      s += adc_read(_BV(REFS0) | ch);
    putch(' ');
    putl((long)((s * (uint32_t)vcc + n * 512UL) / ((uint32_t)n * 1024UL)));
  }
  puts_(" vcc=");
  putl(vcc);
}

static volatile uint16_t edges;
ISR(PCINT1_vect) {
  edges++;
}

static void cmd_count(uint8_t bit, uint16_t ms) {
  edges  = 0;
  PCIFR  = _BV(PCIF1);
  PCMSK1 = _BV(bit);
  PCICR  = _BV(PCIE1);
  sei();
  while(ms--)
    _delay_ms(1);
  cli();
  PCICR  = 0;
  PCMSK1 = 0;
  puts_("c ");
  putl(edges);
}

static uint8_t qidx;

static void cmd_quad(uint16_t us, long steps, uint8_t *st) {
  int8_t dir = steps < 0 ? -1 : 1;
  if(steps < 0)
    steps = -steps;
  while(steps--) {
    qidx  = (qidx + dir) & 3;
    PORTC = (PORTC & ~MASK) | (st[qidx] & MASK);
    for(uint16_t i = 4; i < us; i++)  // the loop itself takes ~4 us a step
      _delay_us(1);
  }
  puts_("q ");
  putl(qidx);
}

static char line[64];

static char *next(char **p) {
  while(**p == ' ')
    (*p)++;
  if(!**p)
    return 0;
  char *s = *p;
  while(**p && **p != ' ')
    (*p)++;
  if(**p)
    *(*p)++ = 0;
  return s;
}

static void run(char *p) {
  char *c = next(&p);
  char *a[6];
  uint8_t n = 0;
  while(n < 6 && (a[n] = next(&p)))
    n++;
  if(!c) {
    return;
  } else if(!strcmp(c, "i")) {
    puts_("fbtest " VERSION " pins=6");
  } else if(!strcmp(c, "o") && n == 2) {
    uint8_t ddr  = strtoul(a[0], 0, 16) & MASK;
    uint8_t port = strtoul(a[1], 0, 16) & MASK;
    PORTC = (PORTC & ~MASK) | port;
    DDRC  = (DDRC & ~MASK) | ddr;
    puts_("ok");
  } else if(!strcmp(c, "d")) {
    puts_("d ");
    puthex(PINC & MASK);
  } else if(!strcmp(c, "a")) {
    long k = n ? atol(a[0]) : 16;
    cmd_analog(k < 1 ? 1 : k > 256 ? 256 : k);
  } else if(!strcmp(c, "q") && n == 6) {
    uint8_t st[4];
    for(uint8_t i = 0; i < 4; i++)
      st[i] = strtoul(a[2 + i], 0, 16);
    cmd_quad(atol(a[0]), atol(a[1]), st);
  } else if(!strcmp(c, "c") && n == 2 && atoi(a[0]) < 6) {
    cmd_count(atoi(a[0]), atol(a[1]));
  } else {
    puts_("err ");
    puts_(c);
  }
  puts_("\r\n");
}

int main(void) {
  DDRC  = 0;  // all lines hi-Z until told otherwise
  PORTC = 0;
  DIDR0 = 0;
  ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);  // 125 kHz
  uart_init();
  puts_("fbtest " VERSION " pins=6\r\n");
  uint8_t len = 0;
  for(;;) {
    while(!(UCSR0A & _BV(RXC0)))
      ;
    char ch = UDR0;
    if(ch == '\r' || ch == '\n') {
      line[len] = 0;
      if(len)
        run(line);
      len = 0;
    } else if(len < sizeof(line) - 1) {
      line[len++] = ch;
    }
  }
}
