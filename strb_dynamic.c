// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#include <stdlib.h>

#include "strb_internal.h"

#if STRB_FREESTANDING
#error "Compile strb_dynamic.c only for dynamic allocation"
#endif

strb_t *strb_init(strb_t *STRB__CONST sb, STRB__CONST size_t n)
{
    assert(sb);
    sb->p.len = sb->p.pos = 0;
    sb->p.flags = sb->p.empty_char = 0;
    sb->p.size = n >= STRB_MAX_SIZE ? STRB_MAX_SIZE : (strbsize_t)(n + 1u);
    sb->p.buf = NULL;
    return sb;
}

_Optional strb_t *strb_alloc(STRB__CONST size_t n)
{
    _Optional strb_t *const sb = malloc(sizeof(strb_t));
    DEBUGF("Alloc buffer of size %zu\n", n);

    if (sb) {
        strb_init(&*sb, n);
        sb->p.flags |= F_HEAP;
    }

    return sb;
}

void strb_destroy(_Optional strb_t *STRB__CONST sb)
{
    if (!sb) {
        return;
    }

    if (!(sb->p.flags & F_EXTERNAL)) {
        free(sb->p.buf);
    }

    if (sb->p.flags & F_HEAP) {
        free(sb);
    }
}

bool strb__grow(strb_t *STRB__CONST sb, STRB__CONST strbsize_t minimum_size)
{
    strbsize_t new_size;
    _Optional char *new_buf;
    const bool first_allocation = !sb->p.buf;

    assert(sb);

    if (sb->p.flags & F_EXTERNAL) {
        DEBUGF("External buffer exhausted\n");
        return false;
    }

    new_size = first_allocation ?
        (sb->p.size < STRB_DFL_SIZE ? STRB_DFL_SIZE : sb->p.size) :
        (sb->p.size <= STRB_MAX_SIZE / STRB_GROW_FACTOR
            ? sb->p.size * STRB_GROW_FACTOR : STRB_MAX_SIZE);

    if (new_size < minimum_size) {
        new_size = minimum_size;
    }

    new_buf = realloc(sb->p.buf, new_size);
    if (!new_buf) {
        return false;
    }

    if (first_allocation) {
        new_buf[0] = '\0';
    }

    sb->p.buf = &*new_buf;
    sb->p.size = new_size;
    DEBUGF("Substituted buffer %p of %" PRIstrbsize " bytes\n", (void *)new_buf,
           new_size);
    return true;
}
