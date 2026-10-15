// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

// Core interface functions, shared by every storage configuration.
#define _GNU_SOURCE
#include "strb_internal.h"

#ifndef strb_strnlen
size_t strb_strnlen(const char *STRB_CONST s, STRB_CONST size_t n)
{
    size_t p = 0;

    while (p < n && s[p]) {
        p++;
    }

    return p;
}
#endif

#if STRB_EXT_STATE
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
        const size_t len = strb_strnlen(buf, size);
        strb_t *const sb = init_use(sbs, (strbsize_t)size, buf,
                                   (strbsize_t)(len < size ? len : 0));

        if (len == size) {
            buf[0] = '\0';
            sb->p.flags |= F_ERR;
        }

        return sb;
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
        const size_t len = strb_strnlen(buf, STRB_MAX_SIZE);
        const char *initial_buf = buf;
        size_t initial_len = len;

        if (len >= STRB_MAX_SIZE) {
            initial_buf = "";
            initial_len = 0;
        }

        {
            strb_t *const sb = init_use(sbs, (strbsize_t)(initial_len + 1u),
                                       (char *)initial_buf,
                                       (strbsize_t)initial_len);

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
#endif // STRB_EXT_STATE

#undef strb_ptr
char *strb_ptr(strb_t *STRB_CONST sb)
{
    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));
    return sb->p.buf;
}

const char *strb_cptr(strb_t const *STRB_CONST sb)
{
    assert(sb);
    return sb->p.buf;
}

size_t strb_len(strb_t const *STRB_CONST sb)
{
    assert(sb);
    return sb->p.len;
}

int strb_set_error(strb_t *STRB_CONST sb)
{
    assert(!(sb->p.flags & F_IS_CONST));
    sb->p.flags |= F_ERR;
    return EOF;
}

int strb_setmode(strb_t *STRB_CONST sb, STRB_CONST int mode)
{
    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));

    if (mode == strb_insert || mode == strb_overwrite) {
        sb->p.flags &= ~(F_CAN_UNPUTC | F_OVERWRITE);

        if (mode == strb_overwrite) {
            sb->p.flags |= F_OVERWRITE;
        }

        return 0;
    } else {
        DEBUGF("Bad mode %d\n", mode);
        return strb_set_error(sb);
    }
}

int strb_getmode(const strb_t *STRB_CONST sb)
{
    assert(sb);
    {
        const int mode = (sb->p.flags & F_OVERWRITE) ? strb_overwrite : strb_insert;
        assert(mode == strb_insert || mode == strb_overwrite);
        return mode;
    }
}

void strb_seek(strb_t *STRB_CONST sb, STRB_CONST size_t pos)
{
    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));
    DEBUGF("Seek to %zu\n", pos);
    // STRB_MAX_SIZE fits in strbsize_t but is not a supported position.
    sb->p.pos = pos < STRB_MAX_SIZE ? (strbsize_t)pos : STRB_MAX_SIZE;
#if STRB_UNPUTC || STRB_RESTORE
    sb->p.flags &= ~(F_CAN_UNPUTC | F_CAN_RESTORE);
#endif
}

size_t strb_tell(strb_t const *STRB_CONST sb)
{
    assert(sb);
    assert(sb->p.pos <= STRB_MAX_SIZE);
    DEBUGF("Pos %" PRIstrbsize ", len %" PRIstrbsize ", size %" PRIstrbsize
           "\n", sb->p.pos, sb->p.len, sb->p.size);
    return sb->p.pos == STRB_MAX_SIZE ? SIZE_MAX : sb->p.pos;
}

int strb_putc(strb_t *STRB_CONST sb, STRB_CONST int c)
{
    return strb_nputc(sb, c, 1);
}

int strb_nputc(strb_t *STRB_CONST sb, STRB_CONST int c, STRB_CONST size_t n)
{
    _Optional char *const buf = strb_write(sb, n);

    if (!buf) {
        return EOF;
    }

    memset(&*buf, c, n);
    // assume F_CAN_RESTORE isn't user-visible. Don't bother calling
    // strb_restore.
    return c;
}

#if STRB_UNPUTC
int strb_unputc(strb_t *STRB_CONST sb)
{
    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));

    if (!(sb->p.flags & F_CAN_UNPUTC)) {
        return strb_set_error(sb);
    }

    assert(sb->p.pos > 0);
    assert(sb->p.pos < STRB_MAX_SIZE);
    assert(sb->p.pos < sb->p.size);
    assert(sb->p.pos <= sb->p.len);

    {
        const strbsize_t new_pos = sb->p.pos - 1;
        const char removed = sb->p.buf[new_pos];

        if (!(sb->p.flags & F_OVERWRITE)) {
            memmove(sb->p.buf + new_pos, sb->p.buf + sb->p.pos,
                    sb->p.len - new_pos);
            --sb->p.len;
        } else {
            sb->p.buf[new_pos] = sb->p.unputc_char;
        }

        sb->p.pos = new_pos;
        sb->p.flags &= ~(F_CAN_UNPUTC | F_CAN_RESTORE);
        return removed;
    }
}
#endif

int strb_nputs(strb_t *STRB_CONST restrict sb,
    const char *STRB_CONST restrict str, STRB_CONST size_t n)
{
    const size_t len = strb_strnlen(str, n);
    _Optional char *const buf = strb_write(sb, len);

    if (!buf) {
        return EOF;
    }

    memcpy(&*buf, str, len); // more efficient than strncpy
    // assume F_CAN_RESTORE isn't user-visible. Don't bother calling
    // strb_restore.
    return 0;
}

int strb_puts(strb_t *STRB_CONST restrict sb, const char *STRB_CONST restrict str)
{
    return strb_nputs(sb, str, STRB_MAX_SIZE);
}

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

        if ((sb->p.flags & F_EXT_STATE) &&
            !(sb->p.flags & (F_EXTERNAL | F_ALLOCATED))) {
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

    return strb_grow(sb, (strbsize_t)(top + n + 1u));
}

_Optional char *strb_write(strb_t *STRB_CONST sb, STRB_CONST size_t n)
{
    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));
    assert(sb->p.len < sb->p.size);
    assert(sb->p.pos <= STRB_MAX_SIZE);
    assert(sb->p.buf[sb->p.len] == '\0');
    DEBUGF("About to write %zu chars\n", n);

    {
        const strbsize_t old_len = sb->p.len, old_pos = sb->p.pos;
        const strbsize_t top = (sb->p.flags & F_OVERWRITE) || old_pos > old_len
                                   ? old_pos
                                   : old_len;

        if (!strb_ensure(sb, n, top)) {
            DEBUGF("No room\n");
            strb_set_error(sb);
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
#if STRB_UNPUTC
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

#if STRB_RESTORE
            sb->p.restore_char = buf[n];
            sb->p.flags |= F_CAN_RESTORE;
            DEBUGF("Stored %d ('%c') at %" PRIstrbsize "\n", sb->p.restore_char,
                   sb->p.restore_char, sb->p.pos);
#endif

#if STRB_UNPUTC
            if (n) {
                sb->p.flags |= F_CAN_UNPUTC;
            }
#endif
            return buf;
        }
    }
}

int strb_split(strb_t *STRB_CONST sb)
{
    _Optional char *const p = strb_write(sb, 0);

    if (!p) {
        return EOF;
    }

    *p = '\0';
    return 0;
}

#if STRB_RESTORE
void strb_restore(strb_t *STRB_CONST sb)
{
    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));

    if (sb->p.flags & F_CAN_RESTORE) {
        DEBUGF("Restored %d ('%c') at %" PRIstrbsize "\n", sb->p.restore_char,
               sb->p.restore_char, sb->p.pos);
        sb->p.buf[sb->p.pos] = sb->p.restore_char;
        sb->p.flags &= ~F_CAN_RESTORE;
    }
}
#endif

void strb_delto(strb_t *STRB_CONST sb, STRB_CONST size_t pos)
{
    size_t hi, lo;

    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));
    {
        const size_t old_pos = strb_tell(sb);

        if (old_pos > pos) {
            lo = pos;
            hi = old_pos;
        } else {
            hi = pos;
            lo = old_pos;
        }
    }
    assert(lo <= hi);

    if (!(sb->p.flags & F_OVERWRITE)) {

        const strbsize_t len = sb->p.len,
                         chi = hi > len ? len : (strbsize_t)hi,
                         clo = lo > len ? len : (strbsize_t)lo;
        assert(clo <= chi);

        memmove(sb->p.buf + clo, sb->p.buf + chi, len + 1u - chi);
        sb->p.len = (strbsize_t)(len - (chi - clo));
    }

    strb_seek(sb, lo);
}

void strb_empty(strb_t *STRB_CONST sb)
{
    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));
    sb->p.len = sb->p.pos = 0;
    sb->p.buf[0] = '\0';
#if STRB_UNPUTC || STRB_RESTORE
    sb->p.flags &= ~(F_CAN_UNPUTC | F_CAN_RESTORE);
#endif
}

int strb_ncpy(strb_t *STRB_CONST restrict sb,
    const char *STRB_CONST restrict str, STRB_CONST size_t n)
{
    strb_empty(sb);
    return strb_nputs(sb, str, n);
}

int strb_cpy(strb_t *STRB_CONST restrict sb, const char *STRB_CONST restrict str)
{
    return strb_ncpy(sb, str, STRB_MAX_SIZE);
}

bool strb_error(strb_t const *STRB_CONST sb)
{
    assert(sb);
    return (sb->p.flags & F_ERR) != 0;
}

void strb_clearerr(strb_t *STRB_CONST sb)
{
    assert(sb);
    assert(!(sb->p.flags & F_IS_CONST));
    sb->p.flags &= ~F_ERR;
}
