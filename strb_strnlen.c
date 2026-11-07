// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

#ifndef strb__strnlen
size_t strb__strnlen(const char *STRB_CONST s, STRB_CONST size_t n)
{
    size_t p = 0;

    while (p < n && s[p]) {
        p++;
    }

    return p;
}
#endif
