# Project:   strb_t
# Copyright 2024 Christopher Bazley
# SPDX-License-Identifier: MIT

# Tools
CC = gcc
Link = gcc

# Toolflags:
CCFlags = -c -Wall -Wextra -Wsign-compare -pedantic -std=c11 -MMD -MP -g -MF $*.d
LinkFlags = -o $@

# Final targets:
all: test statictest freestandingtest

test: strb.o test.o
	$(Link) strb.o test.o $(LinkFlags)

statictest: staticstrb.o statictest.o
	$(Link) staticstrb.o statictest.o $(LinkFlags)

freestandingtest: freestandingstrb.o freestandingtest.o
	$(Link) freestandingstrb.o freestandingtest.o $(LinkFlags)

# Static dependencies:
strb.o:
	$(CC) $(CCFlags) -o strb.o strb.c
test.o:
	$(CC) $(CCFlags) -o test.o test.c

staticstrb.o:
	$(CC) $(CCFlags) -DSTRB_STATIC_ALLOC -o staticstrb.o strb.c
statictest.o:
	$(CC) $(CCFlags) -DSTRB_STATIC_ALLOC -o statictest.o test.c

freestandingstrb.o:
	$(CC) $(CCFlags) -DSTRB_FREESTANDING -o freestandingstrb.o strb.c
freestandingtest.o:
	$(CC) $(CCFlags) -DSTRB_FREESTANDING -o freestandingtest.o test.c

# Dynamic dependencies:
# These files are generated during compilation to track C header #includes.
# It's not an error if they don't exist.
-include strb.d test.d staticstrb.d statictest.d freestandingstrb.d freestandingtest.d
