// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

#if !STRB_EXT_STATE
#error "Compile strb_external.c only when STRB_EXT_STATE is enabled"
#endif

static strb_t *init_use(strbstate_t *const restrict sbs, const strbsize_t size,
    char buf[STRB_SIZE_HINT(size)], const strbsize_t len)
{
    sbs->p.len = sbs->p.pos = len;
    sbs->p.size = size;
    sbs->p.buf = buf;
    sbs->p.flags = F_EXTERNAL | F_EXT_STATE;

#if STRB_UNPUTC
    if (len) {
        sbs->p.flags |= F_CAN_UNPUTC;
    }
#endif
    return (void *)sbs;
}

strb_t *strb_use(strbstate_t *STRB_CONST restrict sbs, size_t size,
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

strb_t *strb_reuse(strbstate_t *STRB_CONST restrict sbs, size_t size,
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

#if STRB_REUSE_CONST
const strb_t *strb_reuse_const(strbstate_t *STRB_CONST restrict sbs,
    const char *STRB_CONST restrict buf)
{
    assert(sbs);
    assert(buf);
    DEBUGF("Reuse const buffer %p\n", (void *)buf);

    {
        const size_t len = strb__strnlen(buf, STRB_MAX_SIZE);
        const char *initial_buf = buf;
        size_t initial_len = len;

        if (len >= STRB_MAX_SIZE) {
            initial_buf = "";
            initial_len = 0;
        }

        {
            strb_t *const sb =
                init_use(sbs, (strbsize_t)(initial_len + 1u),
                         (char *)initial_buf, (strbsize_t)initial_len);

            if (len == STRB_MAX_SIZE) {
                sb->p.flags |= F_ERR;
            }

#ifndef NDEBUG
            sb->p.flags = (char)(sb->p.flags | F_IS_CONST);
#endif
            return sb;
        }
    }
}
#endif // STRB_REUSE_CONST
