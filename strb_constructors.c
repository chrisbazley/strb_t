// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

#if STRB_EXT_STATE
strb_t *strb_init_empty(strbstate_t *STRB_CONST sbs, STRB_CONST strbsize_t hint)
{
    assert(sbs);
    sbs->p.len = sbs->p.pos = 0;

    // Reuse a saved-character byte for the initial writable terminator.
    // Zero-length writes can only save/restore null while this buffer is used;
    // strb_ensure obtains character storage before a nonempty write uses it.
#if STRB_RESTORE
    sbs->p.buf = &sbs->p.restore_char;
#elif STRB_UNPUTC
    sbs->p.buf = &sbs->p.unputc_char;
#else
    // The unsigned length is zero until character storage is acquired. Its
    // first byte can therefore supply the terminator without another field.
    sbs->p.buf = (char *)&sbs->p.len;
#endif
    // Until F_ALLOCATED is set, size records the deferred allocation hint.
    sbs->p.size = hint;
    sbs->p.buf[0] = '\0';
    sbs->p.flags = F_EXT_STATE;
    return (void *)sbs;
}
#else
_Optional strb_t *strb_use(size_t size, char buf[STRB_SIZE_HINT(size)])
{
    assert(buf);
    assert(size > 0);
    DEBUGF("Use buffer %p of size %zu\n", (void *)buf, size);

    if (size > STRB_MAX_SIZE) {
        size = STRB_MAX_SIZE;
    }

    {
        _Optional strb_t *const sb = strb_alloc_metadata(0);

        if (!sb) {
            return NULL;
        }

        sb->p.len = sb->p.pos = 0;
        sb->p.size = (strbsize_t)size;
        sb->p.buf = buf;
        sb->p.flags = F_EXTERNAL;
        buf[0] = '\0';
        return sb;
    }
}

_Optional strb_t *strb_reuse(size_t size, char buf[STRB_SIZE_HINT(size)])
{
    assert(buf);
    assert(size > 0);
    DEBUGF("Reuse buffer %p of size %zu\n", (void *)buf, size);

    if (size > STRB_MAX_SIZE) {
        size = STRB_MAX_SIZE;
    }

    {
        const size_t len = strb_strnlen(buf, size);

        {
            _Optional strb_t *const sb = strb_alloc_metadata(0);

            if (!sb) {
                return NULL;
            }

            sb->p.len = sb->p.pos = (strbsize_t)(len < size ? len : 0);
            sb->p.size = (strbsize_t)size;
            sb->p.buf = buf;
            sb->p.flags = F_EXTERNAL;

            if (len == size) {
                buf[0] = '\0';
                sb->p.flags |= F_ERR;
            }

#if STRB_UNPUTC
            if (sb->p.len) {
                sb->p.flags |= F_CAN_UNPUTC;
            }
#endif
            return sb;
        }
    }
}
#endif

_Optional strb_t *strb_ndup(const char *STRB_CONST str, STRB_CONST size_t n)
{
    const size_t len = strb_strnlen(str, n);

    if (len >= STRB_MAX_SIZE) {
        return NULL;
    }

    {
        _Optional strb_t *const sb = strb_alloc(len + 1u);

        if (!sb) {
            return NULL;
        }

        memcpy(sb->p.buf, str, len); // more efficient than strncpy
        sb->p.buf[len] = '\0';
        sb->p.len = sb->p.pos = (strbsize_t)len;
#if STRB_UNPUTC
        assert(!(sb->p.flags & F_OVERWRITE)); // needn't set unputc_char

        if (len) {
            sb->p.flags |= F_CAN_UNPUTC;
        }
#endif
        return sb;
    }
}

_Optional strb_t *strb_dup(const char *STRB_CONST str)
{
    return strb_ndup(str, STRB_MAX_SIZE);
}
