#pragma once
// The drive's clock lock to LinuxCNC's packets (src/comps/sserial.c, io4's
// clock_scale): once an rt period the smart-serial frt looks how many bytes
// of a packet are waiting and nudges the timer (clock_scale 0.9: ticks
// earlier, 1.1: later) so that this check falls `goal` bytes into the
// packet.
//
// reply_mode 0 (the frt parses): a packet that ended before the check is
// still waiting (available >= goal: late, 0.9).
// reply_mode 1 (the USART's idle interrupt parses at the packet's end): it
// is gone by then, available reads 0. A packet that ended within the last
// frt period (idle_recent) is the same "late" case.
//
// Returns the clock_scale for this check.
#include <stdint.h>

static inline float sserial_sync_scale(uint32_t available, uint32_t goal, int reply_irq, int idle_recent) {
  if(available > goal) {
    return 0.9f;
  }
  if(available < goal && available > 0) {
    return 1.1f;
  }
  if(available == 0 && reply_irq && idle_recent) {
    return 0.9f;
  }
  return 1.0f;
}

// The link timeout (sserial0.timeout frt periods, 5 ms): reset by a parsed
// packet. In reply_mode 1 the frt doesn't parse, so bytes arriving since the
// last frt (the DMA's position moved) are the link alive too. Without that,
// sparse traffic (discovery: hostmot2's config reads, ms apart) let the
// timeout expire, and the frt threw a packet away (rxpos = bufferpos) between
// its last byte and the idle interrupt: discovery failed 16 of 24 times on
// the bench (DEBUGNOTES 2026-11-01 "sserial0.reply_mode").
static inline int sserial_link_alive(int reply_irq, uint32_t bufferpos, uint32_t last_bufferpos) {
  return reply_irq && bufferpos != last_bufferpos;
}
