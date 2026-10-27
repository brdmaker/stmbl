/*
 * Setup wizard: motor feedback (the encoder / resolver on feedback 0).
 *
 * Each type names its config template, the pin that shows its position
 * (for the hand-turn check), and how its resolution is found:
 *  - incremental (A/B/Z) and sin/cos: measured. The wizard sets the drive's
 *    counter to wrap at 65536 (enc_fb0.res) and enables the index print
 *    (enc_fb0.indexprint: "cnt = N" at each index pulse); while the user
 *    turns the shaft by hand, consecutive index counts differ by exactly
 *    the encoder's counts per revolution, and their sign is the counting
 *    direction. No index: the user enters the line count.
 *  - resolver: the pole count is asked (the nameplate says "2 pole", ...).
 *  - serial absolute (Tamagawa smartabs, Mitsubishi, Sanyo Denki): fixed
 *    by the protocol.
 */
#pragma once

typedef enum {
  SU_FB_RES_MEASURED,  // counts from the index pulses
  SU_FB_RES_POLES,     // resolver: pole pairs asked
  SU_FB_RES_FIXED,     // serial absolute: nothing to set
} su_fb_res_t;

typedef struct {
  const char *id;          // session value of feedback.type
  const char *name;        // shown to the user
  const char *link;        // config template ("link enc_fb0")
  const char *extra;       // an extra config line, or NULL
  const char *pos_pin;     // the position the hand-turn check reads
  su_fb_res_t res;
  const char *wiring_col;  // column of the feedback connector table (su_wiring)
  int simulated;           // the host simulator has it (tested end to end)
} su_fb_type_t;

extern const su_fb_type_t su_fb_types[];
extern const int su_fb_ntypes;

const su_fb_type_t *su_fb_find(const char *id);

// the counter wrap used for the measurement
#define SU_FB_MEASURE_WRAP 65536

// collect the "cnt = N" index counts from drive text into cnt (max n);
// returns how many
int su_fb_parse_index(const char *text, long *cnt, int n);

// counts per revolution and direction (+1 counting up) from consecutive
// index counts on a counter wrapping at wrap. 0 ok; -1 fewer than two
// pulses; -2 the steps disagree (the shaft turned back, or no real index)
int su_fb_counts(const long *cnt, int n, long wrap, long *counts, int *dir);

// the encoder's line count from counts (4 per line), 0 when counts isn't a
// multiple of 4
long su_fb_lines(long counts);
