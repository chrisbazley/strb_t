// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

int strb_setmode(strb_t *STRB_CONST sb, STRB_CONST int mode)
{
    assert(sb);

    if (mode == strb_insert || mode == strb_overwrite) {
        const int old_mode = strb_getmode(sb);
        sb->p.flags &= ~(F_CAN_UNPUTC | F_OVERWRITE);

        if (mode == strb_overwrite) {
            sb->p.flags |= F_OVERWRITE;
        }

        return old_mode;
    } else {
        DEBUGF("Bad mode %d\n", mode);
        return strb__seterr(sb);
    }
}

int strb_getmode(const strb_t *STRB_CONST sb)
{
    assert(sb);
    return (sb->p.flags & F_OVERWRITE) ? strb_overwrite : strb_insert;
}
