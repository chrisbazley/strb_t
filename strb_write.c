// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

static bool strb_ensure(strb_t *const sb, const size_t n, const strbsize_t top)
{
    assert(sb);

    if (n >= (size_t)STRB_MAX_SIZE - top) {
        DEBUGF("Integer range exhausted (top=%" PRIstrbsize ", n=%zu)\n", top,
               n);
        return false; // can't represent new length
    }

    {
        strbsize_t size = sb->p.size;

        if (STRB_DEFERRED_ALLOC(sb)) {
            // Only the terminator exists; size is still an allocation hint.
            size = 1;
        }
        {
            const strbsize_t room = size > top ? size - top : 0;
            DEBUGF("Need %zu bytes, have %" PRIstrbsize " bytes\n", n, room);

            if (n < room) {
                return true; // enough room for n chars and terminator
            }
        }
    }

    return STRB_GROW(sb, (strbsize_t)(top + n + 1u));
}

// Prepare whole-string replacement without discarding the old value on failure.
_Optional char *strb__replace(strb_t *STRB_CONST sb, STRB_CONST size_t n)
{
    if (!strb_ensure(sb, n, 0)) {
        strb__seterr(sb);
        return NULL;
    }
    strb_empty(sb);
    sb->p.len = sb->p.pos = (strbsize_t)n;
    sb->p.buf[n] = '\0';
#if STRB_HAS_UNPUTC
    if (n) {
        sb->p.unputc_char = '\0';
        sb->p.flags |= F_CAN_UNPUTC;
    }
#endif
    return sb->p.buf;
}

_Optional char *strb_write(strb_t *STRB_CONST sb, STRB_CONST size_t n)
{
    assert(sb);
    assert(sb->p.len < sb->p.size);
#if STRB_STATIC_ALLOC && !STRB_FREESTANDING
    assert(sb->p.pos <= STRB_MAX_SIZE);
#endif
    assert(sb->p.buf[sb->p.len] == '\0');
    DEBUGF("About to write %zu chars\n", n);

    {
        const strbsize_t old_len = sb->p.len, old_pos = sb->p.pos;
        const strbsize_t top = (sb->p.flags & F_OVERWRITE) || old_pos > old_len
                                   ? old_pos
                                   : old_len;

        if (!strb_ensure(sb, n, top)) {
            DEBUGF("No room\n");
            strb__seterr(sb);
            return NULL;
        }

        assert(old_pos < sb->p.size);

        if (old_pos > old_len) {
            DEBUGF("Zeroing between len %" PRIstrbsize " and pos %" PRIstrbsize
                   "\n",
                   old_len, old_pos + 1u);
            // +1 because there is no null terminator at pos yet
            memset(sb->p.buf + old_len, '\0', old_pos - old_len + 1u);
            sb->p.len = old_pos;
        }

        assert(old_pos <= sb->p.len);

        {
            char *const buf = sb->p.buf + old_pos;

            if (!(sb->p.flags & F_OVERWRITE)) {
                DEBUGF("Moving tail '%s' (%d) from %p to %p\n", buf, *buf,
                       (void *)buf, (void *)(buf + n));
                memmove(buf + n, buf, sb->p.len + 1u - old_pos);
                sb->p.len = (strbsize_t)(sb->p.len + n);
            } else {
#if STRB_HAS_UNPUTC
                // Preserve the final overwritten character only when a new
                // character is prepared. Beyond the old end it is null.
                if (n) {
                    sb->p.unputc_char =
                        old_pos + n - 1u > old_len ? '\0' : buf[n - 1];
                }
#endif
            }

            sb->p.pos = (strbsize_t)(old_pos + n);
            DEBUGF("Pos advanced by %zu to %" PRIstrbsize "\n", n, sb->p.pos);
            assert(sb->p.pos < sb->p.size);

            if (sb->p.pos > sb->p.len) {
                DEBUGF("Bumping length from %" PRIstrbsize " to %" PRIstrbsize
                       "\n",
                       sb->p.len, sb->p.pos);
                sb->p.len = sb->p.pos;
                buf[n] = '\0';
            }

#if STRB_HAS_RESTORE
            sb->p.restore_char = buf[n];
            sb->p.flags |= F_CAN_RESTORE;
            DEBUGF("Stored %d ('%c') at %" PRIstrbsize "\n", sb->p.restore_char,
                   sb->p.restore_char, sb->p.pos);
#endif

#if STRB_HAS_UNPUTC
            if (n) {
                sb->p.flags |= F_CAN_UNPUTC;
            }
#endif
            return buf;
        }
    }
}

#if STRB_HAS_RESTORE
void strb_restore(strb_t *STRB_CONST sb)
{
    assert(sb);

    if (sb->p.flags & F_CAN_RESTORE) {
        DEBUGF("Restored %d ('%c') at %" PRIstrbsize "\n", sb->p.restore_char,
               sb->p.restore_char, sb->p.pos);
        sb->p.buf[sb->p.pos] = sb->p.restore_char;
        sb->p.flags &= ~F_CAN_RESTORE;
    }
}
#endif
