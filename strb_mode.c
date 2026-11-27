// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#define STRB__DEFINE_MODE
#include "strb_internal.h"

int strb_setmode(strb_t *STRB__CONST sb, STRB__CONST int mode)
{
    assert(sb);

    if (mode == strb_insert || mode == strb_overwrite) {
        const int old_mode = strb_getmode(sb);
        sb->p.flags &= ~(STRB__F_CAN_UNPUTC | STRB__F_OVERWRITE);

        if (mode == strb_overwrite) {
            sb->p.flags |= STRB__F_OVERWRITE;
        }

        return old_mode;
    } else {
        DEBUGF("Bad mode %d\n", mode);
        return strb__seterr(sb);
    }
}

#if STRB__C99_INLINE
extern int strb_getmode(const strb_t *sb);
#endif
