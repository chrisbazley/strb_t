// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

int strb_split(strb_t *STRB__CONST sb)
{
    _Optional char *const p = strb_write(sb, 0);

    if (!p) {
        return EOF;
    }

    *p = '\0';
    return 0;
}
