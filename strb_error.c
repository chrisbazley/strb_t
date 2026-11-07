// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

int strb__seterr(strb_t *STRB_CONST sb)
{
    assert(!(sb->p.flags & F_IS_CONST));
    sb->p.flags |= F_ERR;
    return EOF;
}

bool strb_error(strb_t const *STRB_CONST sb)
{
    assert(sb);
    return (sb->p.flags & F_ERR) != 0;
}

void strb_clearerr(strb_t *STRB_CONST sb)
{
    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));
    sb->p.flags &= ~F_ERR;
}
