# Project:   strb_t

# Tools
CC = gcc
Link = gcc

# Toolflags:
CCFlags = -c -Wall -Wextra -Wsign-compare -pedantic -std=c99 -MMD -MP -g -MF $*.d
LinkFlags = -o $@

# Final targets:
all: test tinytest tiniertest

test: strb.o test.o
	$(Link) strb.o test.o $(LinkFlags)

tinytest: tinystrb.o tinytest.o
	$(Link) tinystrb.o tinytest.o $(LinkFlags)

tiniertest: tinierstrb.o tiniertest.o
	$(Link) tinierstrb.o tiniertest.o $(LinkFlags)

# Static dependencies:
strb.o:
	$(CC) $(CCFlags) -DTINY -o strb.o strb.c
test.o:
	$(CC) $(CCFlags) -DTINY -o test.o test.c

tinystrb.o:
	$(CC) $(CCFlags) -DTINY -o tinystrb.o strb.c
tinytest.o:
	$(CC) $(CCFlags) -DTINY -o tinytest.o test.c

tinierstrb.o:
	$(CC) $(CCFlags) -DTINIER -o tinierstrb.o strb.c
tiniertest.o:
	$(CC) $(CCFlags) -DTINIER -o tiniertest.o test.c

# Dynamic dependencies:
# These files are generated during compilation to track C header #includes.
# It's not an error if they don't exist.
-include strb.d test.d tinystrb.d tinytest.d tinierstrb.d tiniertest.d
