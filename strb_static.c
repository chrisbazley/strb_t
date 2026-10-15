// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#include "strb_internal.h"

/** @cond */
struct strb_allocation {
    strb_t state;
    char characters[STRB_MAX_SIZE];
};
/** @endcond */

static struct strb_allocation bufs[STRB_MAX];
static uint8_t nbufs, buf_map;

_Optional strb_t *strb_alloc_metadata(STRB_CONST strbsize_t size)
{
    int free_idx = 0;
    // Every pool slot has a full-sized character array.
    (void)size;

    if (nbufs == STRB_MAX) {
        return NULL;
    }

    for (; free_idx < STRB_MAX; ++free_idx) {
        if (!(buf_map & (1u << free_idx))) {
            break;
        }
    }

    assert(free_idx < STRB_MAX);
    buf_map |= (1u << free_idx);
    ++nbufs;
    bufs[free_idx].state.p.buf = bufs[free_idx].characters;
    return &bufs[free_idx].state;
}

static void free_metadata(strb_t *STRB_CONST sb)
{
    // The state is the first member of its pool slot.
    const ptrdiff_t alloc_idx = (struct strb_allocation *)sb - bufs;
    assert(alloc_idx >= 0);
    assert(alloc_idx < STRB_MAX);
    buf_map = (uint8_t)(buf_map & ~(1u << alloc_idx));
    --nbufs;
}

#if STRB_EXT_STATE
strb_t *strb_init(strbstate_t *STRB_CONST sbs, STRB_CONST size_t n)
{
    (void)n;
    return strb_init_empty(sbs, STRB_MAX_SIZE);
}
#endif

_Optional strb_t *strb_alloc(STRB_CONST size_t n)
{
    _Optional strb_t *const sb = strb_alloc_metadata(0);
    (void)n;

    if (!sb) {
        return NULL;
    }

    sb->p.len = sb->p.pos = 0;
    sb->p.size = STRB_MAX_SIZE;
    sb->p.buf[0] = '\0';
    sb->p.flags = 0;
    return sb;
}

void strb_free(_Optional strb_t *STRB_CONST sb)
{
    if (!sb) {
        return;
    }

    if (sb->p.flags & F_ALLOCATED) {
        // External state holds a character array belonging to another slot.
        struct strb_allocation *const backing =
            (void *)(sb->p.buf - offsetof(struct strb_allocation, characters));
        free_metadata(&backing->state);
    }

    if (!(sb->p.flags & F_EXT_STATE)) {
        free_metadata(&*sb);
    }
}

bool strb_grow(strb_t *STRB_CONST sb, STRB_CONST strbsize_t minimum_size)
{
    _Optional strb_t *backing;
    assert(sb);
    assert(minimum_size <= STRB_MAX_SIZE);
    (void)minimum_size;

    if ((sb->p.flags & (F_EXTERNAL | F_ALLOCATED | F_EXT_STATE)) != F_EXT_STATE) {
        DEBUGF("Fixed buffer exhausted\n");
        return false;
    }

    backing = strb_alloc_metadata(0);

    if (!backing) {
        return false;
    }

    sb->p.buf = backing->p.buf;
    sb->p.buf[0] = '\0';
    sb->p.size = STRB_MAX_SIZE;
    sb->p.flags |= F_ALLOCATED;
    return true;
}
