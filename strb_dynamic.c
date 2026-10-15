// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#include <stdlib.h>

#include "strb_internal.h"

/** @cond */
struct strb_allocation {
    strb_t state;
    char characters[];
};
/** @endcond */

_Optional strb_t *strb_alloc_metadata(STRB_CONST strbsize_t size)
{
    assert(size <= STRB_MAX_INTERNAL_SIZE);
    {
        _Optional struct strb_allocation *const allocation =
            malloc(sizeof(struct strb_allocation) + size);

        if (!allocation) {
            return NULL;
        }

        allocation->state.p.buf = allocation->characters;
        return &allocation->state;
    }
}

static void free_metadata(_Optional strb_t *STRB_CONST sb)
{
    // The state is the first member of its allocation.
    free(sb);
}

static strbsize_t initial_size(size_t n)
{
    if (n < STRB_DFL_SIZE) {
        n = STRB_DFL_SIZE;
    } else if (n >= STRB_MAX_SIZE) {
        n = STRB_MAX_SIZE;
    } else {
        ++n; // allow space for a null terminator
    }

    return (strbsize_t)n;
}

#if STRB_EXT_STATE
strb_t *strb_init(strbstate_t *STRB_CONST sbs, STRB_CONST size_t n)
{
    return strb_init_empty(sbs, initial_size(n));
}
#endif

_Optional strb_t *strb_alloc(STRB_CONST size_t n)
{
    const strbsize_t size = initial_size(n);
    // Keep only small character arrays in the metadata allocation.
    _Optional strb_t *const sb =
        strb_alloc_metadata(size > STRB_MAX_INTERNAL_SIZE ? 0 : size);
    DEBUGF("Alloc buffer of size %zu\n", n);

    if (!sb) {
        return NULL;
    }

    if (size > STRB_MAX_INTERNAL_SIZE) {
        _Optional char *const buf = malloc(size);

        if (!buf) {
            free_metadata(sb);
            return NULL;
        }

        sb->p.buf = &*buf;
        sb->p.flags = F_ALLOCATED;
    } else {
        sb->p.flags = 0;
    }

    sb->p.len = sb->p.pos = 0;
    sb->p.size = size;
    sb->p.buf[0] = '\0';
    return sb;
}

void strb_free(_Optional strb_t *STRB_CONST sb)
{
    if (!sb) {
        return;
    }

    if (sb->p.flags & F_ALLOCATED) {
        free(sb->p.buf);
    }

    if (!(sb->p.flags & F_EXT_STATE)) {
        free_metadata(sb);
    }
}

bool strb_grow(strb_t *STRB_CONST sb, STRB_CONST strbsize_t minimum_size)
{
    strbsize_t new_size;
    _Optional char *new_buf = NULL;

    assert(sb);

    if (sb->p.flags & F_EXTERNAL) {
        DEBUGF("External buffer exhausted\n");
        return false;
    }

    if ((sb->p.flags & F_EXT_STATE) && !(sb->p.flags & F_ALLOCATED)) {
        new_size = sb->p.size;
    } else {
        new_size = sb->p.size <= STRB_MAX_SIZE / STRB_GROW_FACTOR
                       ? sb->p.size * STRB_GROW_FACTOR : STRB_MAX_SIZE;
    }

    if (new_size < minimum_size) {
        new_size = minimum_size;
    }

    if (sb->p.flags & F_ALLOCATED) {
        new_buf = realloc(sb->p.buf, new_size);
    } else {
        new_buf = malloc(new_size);
    }

    if (!new_buf) {
        return false;
    }

    if (!(sb->p.flags & F_ALLOCATED)) {
        memcpy(&*new_buf, sb->p.buf, sb->p.len + 1u);
    }

    sb->p.flags |= F_ALLOCATED;
    sb->p.buf = &*new_buf;
    sb->p.size = new_size;
    DEBUGF("Substituted buffer %p of %" PRIstrbsize " bytes\n", (void *)new_buf,
           new_size);
    return true;
}
