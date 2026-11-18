#pragma once

#include <string.h>

/**
 * Ring buffer for one writer and one reader that may run at the same time
 * (an interrupt and the main loop, or two threads).
 *
 * The writer only stores wr and the reader only stores rd, each after its
 * data (release) and each reading the other's with acquire. Both count
 * modulo 2 * bufsize, so a full buffer (wr - rd = bufsize) and an empty one
 * (wr = rd) differ without a shared length. Before 2026-11-11 both sides
 * changed one `len`, and a lost update dropped, repeated or stalled data
 * (DEBUGNOTES, host/tests/test_ringbuf.c).
 *
 * Writer side: rb_putc, rb_write, rb_free. Reader side: rb_getc, rb_read,
 * rb_undo, rb_getline. rb_len works from either.
 */
struct ringbuf {
  char *buf;            ///< Pointer to buffer memory
  unsigned bufsize;     ///< Size of buffer memory
  unsigned rd;          ///< read count, mod 2 * bufsize (reader only)
  unsigned wr;          ///< write count, mod 2 * bufsize (writer only)
  unsigned skip;        ///< rb_getline: dropping a line that was too long
};

#define RINGBUF(size) \
  { .buf = (char[(size)]){0}, .bufsize = (size) }

/// bytes in the buffer
unsigned rb_len(const struct ringbuf *rb);

/// room left
unsigned rb_free(const struct ringbuf *rb);

/**
 * Read a single byte from a buffer
 *
 * \param   rb    pointer to ringbuffer struct
 * \param   data  pointer to data byte
 * \return  number of bytes read (0 if buffer was empty)
 */
int rb_getc(struct ringbuf *rb, char *data);


/**
 * Write a single byte to a buffer
 *
 * \param   rb    pointer to ringbuffer struct
 * \param   data  pointer to data byte
 * \return  number of bytes written (0 if buffer was full)
 */
int rb_putc(struct ringbuf *rb, const char data);
/**
 * Read from a buffer
 *
 * \return  number of bytes read (up to len)
 */
int rb_read(struct ringbuf *rb, void *data, int len);


/**
 * Write to a buffer
 *
 * \param   rb    pointer to ringbuffer struct
 * \param   data  pointer to data byte
 * \return  number of bytes written (as many as fit)
 *
 */
int rb_write(struct ringbuf *rb, const void *data, int len);

/**
 * undo rb_read: put the last len bytes read back (reader side)
 *
 * \param   rb    pointer to ringbuffer struct
 * \param   len   length to reset read pointer
 * \return  0 on error
 *
 */
int rb_undo(struct ringbuf *rb, int len);

/**
 * Read one line, up to and including '\n'
 *
 * The '\n' is replaced by '\0'. A line needs at most len bytes including
 * its '\n'. A longer line is dropped, up to and including its '\n'.
 * Reader side: safe against a concurrent writer.
 *
 * \param   rb    pointer to ringbuffer struct
 * \param   ptr   destination, len bytes
 * \param   len   size of ptr
 * \return  length including '\n'; 0 if no complete line yet; -1 if a line
 *          was too long and is being dropped
 */
int rb_getline(struct ringbuf *rb, char *ptr, int len);
