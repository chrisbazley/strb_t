// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#define STRB__DEFINE_ERROR
#include "strb_internal.h"

#if STRB__C99_INLINE
extern bool strb_error(strb_t const *sb);
extern void strb_clearerr(strb_t *sb);
#else
int strb__seterr(strb_t *sb)
{
    sb->p.flags |= STRB__F_ERR;
    return EOF;
}
#endif
