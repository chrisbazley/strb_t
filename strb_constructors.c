// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

#if STRB_FREESTANDING
#error "Do not compile strb_constructors.c with STRB_FREESTANDING"
#endif

_Optional strb_t *strb_ndup(const char *STRB__CONST str, STRB__CONST size_t n)
{
    const size_t len = strb__strnlen(str, n);

    if (len >= STRB_MAX_SIZE) {
        return NULL;
    }

    {
        _Optional strb_t *const sb = strb_alloc(len);

        if (sb && strb_ncpy(&*sb, str, len)) {
            strb_destroy(sb);
            return NULL;
        }

        return sb;
    }
}

_Optional strb_t *strb_dup(const char *STRB__CONST str)
{
    return strb_ndup(str, STRB_MAX_SIZE);
}
