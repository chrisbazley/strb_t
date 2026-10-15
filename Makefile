# Project:   strb_t
# Copyright 2024 Christopher Bazley
# SPDX-License-Identifier: MIT

CC = gcc
Link = $(CC)
CCFlags = -c -Wall -Wextra -Wsign-compare -pedantic -std=c11 -MMD -MP -g
LinkFlags = -o $@

dynamic_objects = $(addprefix build-make/dynamic/,strb_core.o strb_constructors.o strb_format.o strb_dynamic.o test.o)
static_objects = $(addprefix build-make/static/,strb_core.o strb_constructors.o strb_format.o strb_static.o test.o)
freestanding_objects = $(addprefix build-make/freestanding/,strb_core.o strb_fixed.o test.o)

.PHONY: all
all: test statictest freestandingtest

test: $(dynamic_objects)
	$(Link) $^ $(LinkFlags)

statictest: $(static_objects)
	$(Link) $^ $(LinkFlags)

freestandingtest: $(freestanding_objects)
	$(Link) $^ $(LinkFlags)

build-make/dynamic/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CCFlags) -o $@ $<

build-make/static/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CCFlags) -DSTRB_STATIC_ALLOC -o $@ $<

build-make/freestanding/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CCFlags) -DSTRB_FREESTANDING -o $@ $<

-include $(dynamic_objects:.o=.d) $(static_objects:.o=.d) $(freestanding_objects:.o=.d)
