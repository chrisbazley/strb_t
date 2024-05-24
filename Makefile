# Project:   strb_t

# Tools
CC = gcc
Link = gcc

# Toolflags:
CCCommonFlags = -c -Wall -Wextra -Wsign-compare -pedantic -std=c99 -MMD -MP
CCFlags = $(CCCommonFlags) -DNDEBUG -O3 -MF $*.d
CCDebugFlags = $(CCCommonFlags) -g -MF $*D.d
LinkCommonFlags = -o $@
LinkFlags = $(LinkCommonFlags) $(addprefix -l,$(ReleaseLibs))
LinkDebugFlags = $(LinkCommonFlags) $(addprefix -l,$(DebugLibs))

ObjectList = strb test

DebugObjectsStrb = $(addsuffix .debug,$(ObjectList))
ReleaseObjectsStrb = $(addsuffix .o,$(ObjectList))
DebugLibs = 
ReleaseLibs = 

# Final targets:
all: Test TestD

Test: $(ReleaseObjectsStrb)
	$(Link) $(ReleaseObjectsStrb) $(LinkFlags)

TestD: $(DebugObjectsStrb)
	$(Link) $(DebugObjectsStrb) $(LinkDebugFlags)


# User-editable dependencies:
.SUFFIXES: .o .c .debug
.c.debug:
	$(CC) $(CCDebugFlags) -o $@ $<
.c.o:
	$(CC) $(CCFlags) -o $@ $<

# Static dependencies:

# Dynamic dependencies:
# These files are generated during compilation to track C header #includes.
# It's not an error if they don't exist.
-include $(addsuffix .d,$(ObjectList))
-include $(addsuffix D.d,$(ObjectList))
