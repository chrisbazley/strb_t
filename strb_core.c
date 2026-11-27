// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

// Accessors and state updates without allocation or string-operation
// dependencies.
#define _GNU_SOURCE
#define STRB__DEFINE_CORE
#include "strb_internal.h"

#if STRB__C99_INLINE
#undef strb_ptr
extern char *strb_ptr(strb_t *sb);
extern const char *strb_cptr(strb_t const *sb);
extern size_t strb_len(strb_t const *sb);
#endif

void strb_empty(strb_t *STRB__CONST sb)
{
    assert(sb);
    sb->p.len = sb->p.pos = 0;
    if (sb->p.buf) {
        sb->p.buf[0] = '\0';
    }
#if STRB_HAS_UNPUTC || STRB_HAS_RESTORE
    sb->p.flags &= ~(STRB__F_CAN_UNPUTC | STRB__F_CAN_RESTORE);
#endif
}
