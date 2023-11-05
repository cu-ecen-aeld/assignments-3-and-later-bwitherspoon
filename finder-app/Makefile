CC := gcc
CC := $(CROSS_COMPILE)$(CC)

CFLAGS = -g -Wall

.PHONY: all
all: writer

writer: writer.c

.PHONY: clean
clean:
	-${RM} writer
