// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#define STRB__TRACE_POSITION(sb) \
    DEBUGF("Pos %" PRIstrbsize ", len %" PRIstrbsize ", size %" PRIstrbsize "\n", \
           (sb)->p.pos, (sb)->p.len, (sb)->p.size)
#define STRB__DEFINE_POS
#include "strb_internal.h"

size_t strb_seek(strb_t *STRB__CONST sb, STRB__CONST size_t pos)
{
    const size_t old_pos = strb_tell(sb);
    assert(sb);
    DEBUGF("Seek to %zu\n", pos);
    // STRB_MAX_SIZE fits in strbsize_t but is not a supported position.
    sb->p.pos = pos < STRB_MAX_SIZE ? (strbsize_t)pos : STRB_MAX_SIZE;
#if STRB_HAS_UNPUTC || STRB_HAS_RESTORE
    sb->p.flags &= ~(STRB__F_CAN_UNPUTC | STRB__F_CAN_RESTORE);
#endif
    return old_pos;
}

#if STRB__C99_INLINE
extern size_t strb_tell(strb_t const *sb);
#endif
