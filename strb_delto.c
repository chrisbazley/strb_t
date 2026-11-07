// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

void strb_delto(strb_t *STRB_CONST sb, STRB_CONST size_t pos)
{
    size_t hi, lo;

    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));
    {
        const size_t old_pos = strb_tell(sb);

        if (old_pos > pos) {
            lo = pos;
            hi = old_pos;
        } else {
            hi = pos;
            lo = old_pos;
        }
    }
    assert(lo <= hi);

    if (!(sb->p.flags & F_OVERWRITE)) {
        const strbsize_t len = sb->p.len, chi = hi > len ? len : (strbsize_t)hi,
                         clo = lo > len ? len : (strbsize_t)lo;
        assert(clo <= chi);

        memmove(sb->p.buf + clo, sb->p.buf + chi, len + 1u - chi);
        sb->p.len = (strbsize_t)(len - (chi - clo));
    }

    strb_seek(sb, lo);
}
