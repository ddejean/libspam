# Copyright (C) 2011 par Damien Dejean

# Files to compile
FILES=$(wildcard *.c)

# crt0.o must be the first object linked
# rmcode.o must not be included
OBJS=$(patsubst %.c,%.o,$(FILES))

CC=gcc
CFLAGS=-g -pipe -Wall -Wextra -Werror

# Targets that are not files
.PHONY: clean all 

# The default target
all: libspam

libspam: $(OBJS)
	$(CC) -o libspam $(OBJS) $(CFLAGS) 

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS) -D_DEBUG_

clean:
	rm -f *~ *.o libspam

