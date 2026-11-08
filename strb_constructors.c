// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

#if STRB_FREESTANDING
#error "Do not compile strb_constructors.c with STRB_FREESTANDING"
#endif

_Optional strb_t *strb_ndup(const char *STRB_CONST str, STRB_CONST size_t n)
{
    const size_t len = strb__strnlen(str, n);

    if (len >= STRB_MAX_SIZE) {
        return NULL;
    }

    {
        _Optional strb_t *const sb = strb_alloc(len + 1u);

        if (!sb) {
            return NULL;
        }

        memcpy(sb->p.buf, str, len); // more efficient than strncpy
        sb->p.buf[len] = '\0';
        sb->p.len = sb->p.pos = (strbsize_t)len;
#if STRB_HAS_UNPUTC
        assert(!(sb->p.flags & F_OVERWRITE)); // needn't set unputc_char

        if (len) {
            sb->p.flags |= F_CAN_UNPUTC;
        }
#endif
        return sb;
    }
}

_Optional strb_t *strb_dup(const char *STRB_CONST str)
{
    return strb_ndup(str, STRB_MAX_SIZE);
}
