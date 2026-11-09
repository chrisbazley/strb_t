// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

int strb_putc(strb_t *STRB__CONST sb, STRB__CONST int c)
{
    return strb_nputc(sb, c, 1);
}

int strb_nputc(strb_t *STRB__CONST sb, STRB__CONST int c, STRB__CONST size_t n)
{
    _Optional char *const buf = strb_write(sb, n);

    if (!buf) {
        return EOF;
    }

    memset(&*buf, c, n);
    // assume F_CAN_RESTORE isn't user-visible. Don't bother calling
    // strb_restore.
    return c;
}
