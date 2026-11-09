// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

int strb_nputs(strb_t *STRB__CONST restrict sb,
    const char *STRB__CONST restrict str, STRB__CONST size_t n)
{
    const size_t len = strb__strnlen(str, n);
    _Optional char *const buf = strb_write(sb, len);

    if (!buf) {
        return EOF;
    }

    memcpy(&*buf, str, len); // more efficient than strncpy
    // assume F_CAN_RESTORE isn't user-visible. Don't bother calling
    // strb_restore.
    return 0;
}

int strb_puts(strb_t *STRB__CONST restrict sb,
    const char *STRB__CONST restrict str)
{
    return strb_nputs(sb, str, STRB_MAX_SIZE);
}

int strb_ncpy(strb_t *STRB__CONST restrict sb,
    const char *STRB__CONST restrict str, STRB__CONST size_t n)
{
    const size_t len = strb__strnlen(str, n);
    _Optional char *const buf = strb__replace(sb, len);

    if (!buf) {
        return EOF;
    }
    memcpy(&*buf, str, len);
    return 0;
}

int strb_cpy(strb_t *STRB__CONST restrict sb,
    const char *STRB__CONST restrict str)
{
    return strb_ncpy(sb, str, STRB_MAX_SIZE);
}
