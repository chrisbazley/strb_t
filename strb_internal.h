// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#ifndef STRB_INTERNAL_H
#define STRB_INTERNAL_H

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if DEBUGOUT
#define DEBUGF(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUGF(...)
#endif

#include "strb.h"

#ifdef FORTIFY
#include <stdlib.h>
#include "fortify.h"
#endif

#ifdef USE_OPTIONAL
#if !STRB_FREESTANDING
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static inline void strb__optional_free(_Optional void *const x)
{
    free((void *)x);
}
#undef free
#define free(x) strb__optional_free(x)

static inline _Optional void *strb__optional_malloc(const size_t n)
{
    return malloc(n);
}
#undef malloc
#define malloc(n) strb__optional_malloc(n)

static inline _Optional void *strb__optional_realloc(_Optional void *const p, const size_t n)
{
    return realloc((void *)p, n);
}
#undef realloc
#define realloc(p, n) strb__optional_realloc(p, n)

// SDCC's library omits vsnprintf. Unformatted dynamic buffers still work.
#ifndef __SDCC
static inline int strb__optional_vsnprintf(_Optional char *const buffer, const size_t sz,
                                     const char *const format, va_list vlist)
{
    return vsnprintf((char *)buffer, sz, format, vlist);
}
#undef vsnprintf
#define vsnprintf(buffer, sz, format, vlist)                                   \
    strb__optional_vsnprintf(buffer, sz, format, vlist)
#endif // !__SDCC
#endif // !STRB_FREESTANDING
#endif

// cc65 rejects top-level parameter qualifiers that differ from a prototype.
#if defined(__CC65__)
#define STRB__CONST
#else
#define STRB__CONST const
#endif

#if STRB_HAS_UNPUTC
#define STRB__F_CAN_UNPUTC (1 << 0)
#else
#define STRB__F_CAN_UNPUTC 0
#endif
#if STRB_HAS_RESTORE
#define STRB__F_CAN_RESTORE (1 << 2)
#else
#define STRB__F_CAN_RESTORE 0
#endif
#define STRB__F_HEAP (1 << 4)
#define STRB__F_EXTERNAL (1 << 5)

// Private external functions use strb__; declarations stay in this header.
#if STRB_FREESTANDING
// Fixed storage cannot grow. None of the arguments is evaluated.
#define STRB__GROW(sb, n, top) false
#else
bool strb__grow(strb_t *sb, size_t n, strbsize_t top);
#define STRB__GROW(sb, n, top) strb__grow(sb, n, top)
#endif
#if STRB__C99_INLINE
static inline int strb__seterr(strb_t *sb)
{
    sb->p.flags |= STRB__F_ERR;
    return EOF;
}
#else
int strb__seterr(strb_t *sb);
#endif
_Optional char *strb__replace(strb_t *sb, size_t n);

#if defined(__CC65__) || \
    (!defined(_MSC_VER) && \
     (!defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L) && \
     (!defined(_POSIX_VERSION) || _POSIX_VERSION < 200809L))
size_t strb__strnlen(const char *s, size_t n);
#else
#define strb__strnlen strnlen
#endif

#endif
