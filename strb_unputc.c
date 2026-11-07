// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

#if !STRB_UNPUTC
#error "Compile strb_unputc.c only when STRB_UNPUTC is enabled"
#endif

int strb_unputc(strb_t *STRB_CONST sb)
{
    assert(sb);

    if (!(sb->p.flags & F_CAN_UNPUTC)) {
        return strb__seterr(sb);
    }

    assert(sb->p.pos > 0);
    assert(sb->p.pos < STRB_MAX_SIZE);
    assert(sb->p.pos < sb->p.size);
    assert(sb->p.pos <= sb->p.len);

    {
        const strbsize_t new_pos = sb->p.pos - 1;
        const char removed = sb->p.buf[new_pos];

        if (!(sb->p.flags & F_OVERWRITE)) {
            memmove(sb->p.buf + new_pos, sb->p.buf + sb->p.pos,
                    sb->p.len - new_pos);
            --sb->p.len;
        } else {
            sb->p.buf[new_pos] = sb->p.unputc_char;
        }

        sb->p.pos = new_pos;
        sb->p.flags &= ~(F_CAN_UNPUTC | F_CAN_RESTORE);
        return removed;
    }
}
