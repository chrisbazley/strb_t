// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#ifndef STRB_INTERNAL_H
#define STRB_INTERNAL_H

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "strb.h"

#if DEBUGOUT
#define DEBUGF(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUGF(...)
#endif

// cc65 rejects top-level parameter qualifiers that differ from a prototype.
#if defined(__CC65__)
#define STRB_CONST
#else
#define STRB_CONST const
#endif

#if STRB_UNPUTC
#define F_CAN_UNPUTC (1 << 0)
#else
#define F_CAN_UNPUTC 0
#endif

#define F_ERR (1 << 1)

#if STRB_RESTORE
#define F_CAN_RESTORE (1 << 2)
#else
#define F_CAN_RESTORE 0
#endif

#define F_OVERWRITE (1 << 3)
#define F_ALLOCATED (1 << 4)
#define F_EXTERNAL (1 << 5)
#define F_EXT_STATE (1 << 6)
#if STRB_REUSE_CONST
#define F_IS_CONST (1 << 7)
#else
#define F_IS_CONST 0
#endif

struct strb_t {
    strbprivate_t p;
};

// Private external functions use strb__; declarations stay in this header.
#if STRB_FREESTANDING
// A zero mask avoids unreachable-code diagnostics for fixed storage.
#define STRB_DEFERRED_ALLOC(sb) (((sb)->p.flags & 0) != 0)
// Fixed storage cannot grow. Neither argument is evaluated.
#define STRB_GROW(sb, minimum_size) false
#else
#define STRB_DEFERRED_ALLOC(sb) \
    (((sb)->p.flags & (F_EXT_STATE | F_EXTERNAL | F_ALLOCATED)) == F_EXT_STATE)
bool strb__grow(strb_t *sb, strbsize_t minimum_size);
#define STRB_GROW(sb, minimum_size) strb__grow(sb, minimum_size)
#endif
int strb__seterr(strb_t *sb);
void strb__empty(strb_t *sb);

#if defined(__CC65__) || \
    (!defined(_MSC_VER) && \
     (!defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L) && \
     (!defined(_POSIX_VERSION) || _POSIX_VERSION < 200809L))
size_t strb__strnlen(const char *s, size_t n);
#else
#define strb__strnlen strnlen
#endif

#if !STRB_FREESTANDING
_Optional strb_t *strb__allocmetadata(strbsize_t size);
#if STRB_EXT_STATE
strb_t *strb__initempty(strbstate_t *sbs, strbsize_t hint);
#endif
#endif

#endif
