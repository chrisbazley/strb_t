// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

// Accessors and state updates without allocation or string-operation
// dependencies.
#define _GNU_SOURCE
#include "strb_internal.h"

#undef strb_ptr
char *strb_ptr(strb_t *STRB_CONST sb)
{
    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));
    return sb->p.buf;
}

#if defined(NDEBUG) && defined(__GNUC__) && defined(__ELF__)
// With assertions disabled, both accessors have the same implementation.
const char *strb_cptr(strb_t const *sb) __attribute__((alias("strb_ptr")));
#else
const char *strb_cptr(strb_t const *STRB_CONST sb)
{
#ifdef NDEBUG
    return strb_ptr((strb_t *)sb);
#else
    assert(sb);
    return sb->p.buf;
#endif
}
#endif

size_t strb_len(strb_t const *STRB_CONST sb)
{
    assert(sb);
    return sb->p.len;
}

void strb__empty(strb_t *STRB_CONST sb)
{
    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));
    sb->p.len = sb->p.pos = 0;
    sb->p.buf[0] = '\0';
#if STRB_UNPUTC || STRB_RESTORE
    sb->p.flags &= ~(F_CAN_UNPUTC | F_CAN_RESTORE);
#endif
}

#if !STRB_FREESTANDING
strb_t *strb__initempty(strbstate_t *STRB_CONST sbs, STRB_CONST strbsize_t hint)
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
#endif
