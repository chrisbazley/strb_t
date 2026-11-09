// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

static strb_t *init_use(strbstate_t *const restrict sbs, const strbsize_t size,
    char buf[STRB_SIZE_HINT(size)], const strbsize_t len)
{
    sbs->p.len = sbs->p.pos = len;
    sbs->p.size = size;
    sbs->p.buf = buf;
    sbs->p.flags = F_EXTERNAL | F_EXT_STATE;

#if STRB_HAS_UNPUTC
    if (len) {
        sbs->p.flags |= F_CAN_UNPUTC;
    }
#endif
    return (void *)sbs;
}

strb_t *strb_use(strbstate_t *STRB__CONST restrict sbs, size_t size,
    char buf[STRB_SIZE_HINT(size)])
{
    assert(sbs);
    assert(buf);
    assert(size > 0);
    DEBUGF("Use buffer %p of size %zu\n", (void *)buf, size);

    if (size > STRB_MAX_SIZE) {
        size = STRB_MAX_SIZE;
    }

    buf[0] = '\0';
    return init_use(sbs, (strbsize_t)size, buf, 0);
}

strb_t *strb_reuse(strbstate_t *STRB__CONST restrict sbs, size_t size,
    char buf[STRB_SIZE_HINT(size)])
{
    assert(sbs);
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
            strb_t *const sb =
                init_use(sbs, (strbsize_t)size, buf, (strbsize_t)len);

            if (failed) {
                sb->p.flags |= F_ERR;
            }

            return sb;
        }
    }
}
