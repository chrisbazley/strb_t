// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

// Accessors and state updates without allocation or string-operation
// dependencies.
#define _GNU_SOURCE
#include "strb_internal.h"

#undef strb_ptr
char *strb_ptr(strb_t *STRB__CONST sb)
{
    assert(sb);
    return STRB__BUF(sb);
}

#if defined(NDEBUG) && defined(__GNUC__) && defined(__ELF__)
// With assertions disabled, both accessors have the same implementation.
const char *strb_cptr(strb_t const *sb) __attribute__((alias("strb_ptr")));
#else
const char *strb_cptr(strb_t const *STRB__CONST sb)
{
    return strb_ptr((strb_t *)sb);
}
#endif

size_t strb_len(strb_t const *STRB__CONST sb)
{
    assert(sb);
    return sb->p.len;
}

void strb_empty(strb_t *STRB__CONST sb)
{
    assert(sb);
    sb->p.len = sb->p.pos = 0;
    if (sb->p.buf) {
        sb->p.buf[0] = '\0';
    }
#if STRB_HAS_UNPUTC || STRB_HAS_RESTORE
    sb->p.flags &= ~(F_CAN_UNPUTC | F_CAN_RESTORE);
#endif
}
