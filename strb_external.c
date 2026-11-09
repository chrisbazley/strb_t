// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

static strb_t *init_use(strb_t *const restrict sb, const strbsize_t size,
    char buf[STRB_SIZE_HINT(size)], const strbsize_t len)
{
    sb->p.len = sb->p.pos = len;
    sb->p.size = size;
    sb->p.buf = buf;
    sb->p.flags = F_EXTERNAL;

#if STRB_HAS_UNPUTC
    if (len) {
        sb->p.flags |= F_CAN_UNPUTC;
    }
#endif
    sb->p.empty_char = 0;
    return sb;
}

strb_t *strb_use(strb_t *STRB__CONST restrict sb, size_t size,
    char buf[STRB_SIZE_HINT(size)])
{
    assert(sb);
    assert(buf);
    assert(size > 0);
    DEBUGF("Use buffer %p of size %zu\n", (void *)buf, size);

    if (size > STRB_MAX_SIZE) {
        size = STRB_MAX_SIZE;
    }

    buf[0] = '\0';
    return init_use(sb, (strbsize_t)size, buf, 0);
}

strb_t *strb_reuse(strb_t *STRB__CONST restrict sb, size_t size,
    char buf[STRB_SIZE_HINT(size)])
{
    assert(sb);
    assert(buf);
    assert(size > 0);
    DEBUGF("Reuse buffer %p of size %zu\n", (void *)buf, size);

    if (size > STRB_MAX_SIZE) {
        size = STRB_MAX_SIZE;
    }

    {
        size_t len = strb__strnlen(buf, size);
        const bool failed = len == size;

        if (failed) {
            buf[0] = '\0';
            len = 0;
        }

        {
            strb_t *const result =
                init_use(sb, (strbsize_t)size, buf, (strbsize_t)len);

            if (failed) {
                result->p.flags |= F_ERR;
            }

            return result;
        }
    }
}
