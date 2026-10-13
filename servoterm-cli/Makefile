# servoterm-cli: command-line client for a real stmbl drive over USB serial.
#
# Usage:
#   make            build ./servoterm-cli
#   make clean

CC     ?= cc
CFLAGS ?= -O1 -g
CFLAGS += -std=gnu11 -Wall

all: servoterm-cli

servoterm-cli: servoterm-cli.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f servoterm-cli

.PHONY: all clean
