// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

#if !STRB_HAS_UNPUTC
#error "Compile strb_unputc.c only when STRB_HAS_UNPUTC is enabled"
#endif

int strb_unputc(strb_t *STRB__CONST sb)
{
    _Optional char *buf;

    assert(sb);
    assert(sb->p.pos > 0);
    assert(sb->p.pos < STRB_MAX_SIZE);
    assert(sb->p.pos < sb->p.size);
    assert(sb->p.pos <= sb->p.len);

    buf = sb->p.buf;

    if (!(sb->p.flags & F_CAN_UNPUTC) || !buf) {
        return strb__seterr(sb);
    }
    else
    {
        const strbsize_t new_pos = sb->p.pos - 1;
        const char removed = buf[new_pos];

        if (!(sb->p.flags & F_OVERWRITE)) {
            memmove(buf + new_pos, buf + sb->p.pos,
                    sb->p.len - new_pos);
            --sb->p.len;
        } else {
            buf[new_pos] = sb->p.unputc_char;
        }

        sb->p.pos = new_pos;
        sb->p.flags &= ~(F_CAN_UNPUTC | F_CAN_RESTORE);
        return removed;
    }
}
