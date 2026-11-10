// appendconf's append: one console line plus '\n' onto the RAM config.
//
// The F4's appendconf passed strncat sizeof(config) - 1 as the count, but
// strncat's count is the room left, not the buffer size: once the config
// was nearly full a line was written past config[] (DEBUGNOTES 2026-11-10).
// A line that doesn't fit whole is refused and the config left unchanged,
// so a full config never ends in half a line.
#pragma once
#include <stddef.h>
#include <string.h>

// returns 0 when appended, -1 when line + '\n' doesn't fit
static inline int conf_append(char *buf, size_t size, const char *line) {
  size_t used = strnlen(buf, size);
  if(used >= size) {
    return -1;  // not terminated: leave it alone
  }
  size_t len = strlen(line);
  if(len + 1 > size - 1 - used) {
    return -1;
  }
  memcpy(buf + used, line, len);
  buf[used + len]     = '\n';
  buf[used + len + 1] = '\0';
  return 0;
}
