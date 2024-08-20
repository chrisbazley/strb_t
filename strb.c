// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#if DEBUGOUT
#define DEBUGF(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUGF(...)
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "strb.h"

#define _Optional

#if STRB_UNPUTC
#define F_CAN_UNPUTC (1<<0)
#else
#define F_CAN_UNPUTC 0
#endif

#define F_ERR (1<<1)

#if STRB_RESTORE
#define F_CAN_RESTORE (1<<2)
#else
#define F_CAN_RESTORE 0
#endif

#define F_OVERWRITE (1<<3)
#if !STRB_STATIC_ALLOC && !STRB_FREESTANDING
#define F_ALLOCATED (1<<4)
#endif
#define F_EXTERNAL (1<<5)
#define F_AUTOFREE (1<<6)

/** String buffer object */
struct strb_t {
    /** String buffer state */
    strbprivate_t p;
#if STRB_FREESTANDING
    // No internal storage
#elif STRB_STATIC_ALLOC
    /** Internal string buffer */
    char internal[STRB_MAX_SIZE];
#else
    /** Internal string buffer */
    char internal[];
#endif
};

#if STRB_STATIC_ALLOC
static strb_t bufs[STRB_MAX];
static uint8_t nbufs, buf_map;
#endif

#if STRB_STATIC_ALLOC || STRB_FREESTANDING
// not provided by cc65
size_t strnlen(const char *s, size_t n)
{
    size_t p = 0;
    while (p < n && s[p])
        p++;
    return p;
}
#endif

#if STRB_STATIC_ALLOC
static _Optional strb_t *alloc_metadata(void)
{
    int free_idx = 0;

    if (nbufs == STRB_MAX)
        return NULL;

    for (; free_idx < STRB_MAX; ++free_idx) {
        if (!(buf_map & (1u << free_idx)))
            break;
    }

    assert(free_idx < STRB_MAX);
    buf_map |= (1u << free_idx);
    nbufs++;

    return &bufs[free_idx];
}
#define alloc_metadata(size) alloc_metadata()

static void free_metadata(_Optional strb_t *sb)
{
    if (sb)
    {
        ptrdiff_t alloc_idx = sb - bufs;
        assert(alloc_idx >= 0);
        assert(alloc_idx < STRB_MAX);
        buf_map &= ~(1u << alloc_idx);
        nbufs--;
    }
}
#elif !STRB_FREESTANDING
static _Optional strb_t *alloc_metadata(strbsize_t size)
{
    assert(size <= STRB_MAX_INTERNAL_SIZE);
    return malloc(sizeof(strb_t) + (size * sizeof(((strb_t *)0)->internal[0])));
}

static void free_metadata(_Optional strb_t *sb)
{
    free(sb);
}
#endif

#if STRB_EXT_STATE
static strb_t *init_use(strbstate_t *restrict sb, strbsize_t size, char buf[STRB_SIZE_HINT(size)], strbsize_t len)
{
    sb->p.len = sb->p.pos = len;
    sb->p.size = size;
    sb->p.buf = buf;
    sb->p.flags = F_EXTERNAL | F_AUTOFREE;

#if STRB_UNPUTC
    if (len)
        sb->p.flags |= F_CAN_UNPUTC;
#endif
    return (void *)sb;
}

strb_t *strb_use(strbstate_t *restrict sb, size_t size, char buf[STRB_SIZE_HINT(size)])
{
    assert(sb);
    assert(buf);
    assert(size > 0);
    DEBUGF("Use buffer %p of size %zu\n", buf, size);

    if (size > STRB_MAX_SIZE)
        size = STRB_MAX_SIZE;

    buf[0] = '\0';
    return init_use(sb, size, buf, 0);
}

_Optional strb_t *strb_reuse(strbstate_t *restrict sb, size_t size, char buf[STRB_SIZE_HINT(size)])
{
    assert(sb);
    assert(buf);
    assert(size > 0);
    DEBUGF("Reuse buffer %p of size %zu\n", buf, size);

    if (size > STRB_MAX_SIZE)
        size = STRB_MAX_SIZE;

    {
        size_t len = strnlen(buf, size);
        if (len == size) {
            // Could be outside of the caller's control because of STRB_MAX_SIZE.
            // Don't want to force use of strb_error after any call.
            return NULL;
        }

        return init_use(sb, size, buf, len);
    }
}
#endif // STRB_EXT_STATE

#if !STRB_FREESTANDING
#if !STRB_EXT_STATE
_Optional strb_t *strb_use(size_t size, char buf[STRB_SIZE_HINT(size)])
{
    assert(buf);
    assert(size > 0);
    DEBUGF("Use buffer %p of size %zu\n", buf, size);

    if (size > STRB_MAX_SIZE)
        size = STRB_MAX_SIZE;

    {
        _Optional strb_t *sb = alloc_metadata(0);
        if (!sb) return NULL;

        sb->p.len = sb->p.pos = 0;
        sb->p.size = size;
        sb->p.buf = buf;
        sb->p.flags = F_EXTERNAL;
        buf[0] = '\0';
        return sb;
    }
}

_Optional strb_t *strb_reuse(size_t size, char buf[STRB_SIZE_HINT(size)])
{
    _Optional strb_t *sb = NULL;

    assert(buf);
    assert(size > 0);
    DEBUGF("Reuse buffer %p of size %zu\n", buf, size);

    if (size > STRB_MAX_SIZE)
        size = STRB_MAX_SIZE;

    {
        size_t len = strnlen(buf, size);
        if (len == size) {
            // Could be outside of the caller's control because of STRB_MAX_SIZE.
            // Don't want to force use of strb_error after any call.
            return NULL;
        }

        sb = alloc_metadata(0);
        if (!sb) return NULL;

        sb->p.len = sb->p.pos = len;
        sb->p.size = size;
        sb->p.buf = buf;
        sb->p.flags = F_EXTERNAL;

#if STRB_UNPUTC
        if (len)
            sb->p.flags |= F_CAN_UNPUTC;
#endif
    }
    return sb;
}
#endif // !STRB_EXT_STATE

_Optional strb_t *strb_alloc(size_t n)
{
    DEBUGF("Alloc buffer of size %zu\n", n);

#if STRB_STATIC_ALLOC
    n = STRB_MAX_SIZE;
#else
    if (n < STRB_DFL_SIZE)
        n = STRB_DFL_SIZE;
    else if (n >= STRB_MAX_SIZE)
        n = STRB_MAX_SIZE;
    else
        ++n; // allow space for a null terminator
#endif
    {
        // Don't allocate huge internal strings because the storage can't be recovered
        _Optional strb_t *sb = alloc_metadata(
            n > STRB_MAX_INTERNAL_SIZE ? 0 : n);
        if (!sb) return NULL;
#if !STRB_STATIC_ALLOC
        if (n > STRB_MAX_INTERNAL_SIZE) {
            DEBUGF("Oversize buffer of %zu characters\n", n);
            sb->p.buf = malloc(n * sizeof(*sb->p.buf));
            if (!sb->p.buf) {
                free_metadata(sb);
                return NULL;
            }
            sb->p.flags = F_ALLOCATED;
        } else
#endif
        {
            DEBUGF("Internal buffer of %zu characters\n", n);
            sb->p.buf = sb->internal;
            sb->p.flags = 0;
        }

        sb->p.len = sb->p.pos = 0;
        sb->p.size = n;
        sb->p.buf[0] = '\0';
        return sb;
    }
}

_Optional strb_t *strb_ndup(const char *str, size_t n)
{
    size_t len = strnlen(str, n);
    if (len >= STRB_MAX_SIZE)
        return NULL;

    {
        strb_t *sb = strb_alloc(len + 1);
        if (!sb)
                return NULL;

        memcpy(sb->p.buf, str, len); // more efficient than strncpy
        sb->p.buf[len] = '\0';
        sb->p.len = sb->p.pos = len;
#if STRB_UNPUTC
        assert(!(sb->p.flags & F_OVERWRITE)); // needn't set unputc_char
        if (len)
            sb->p.flags |= F_CAN_UNPUTC;
#endif
        return sb;
    }
}

_Optional strb_t *strb_vaprintf(const char *restrict format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    {
        strb_t *sb = NULL;
        int len = vsnprintf(NULL, 0, format, args);
        if (len < 0 || (size_t)len >= STRB_MAX_SIZE)
            return NULL; // formatting failed or result too long

        sb = strb_alloc((size_t)len + 1);
        if (!sb)
                return NULL;

        assert(sb->p.size > len);
        vsprintf(sb->p.buf, format, args_copy);
        va_end(args_copy);

        sb->p.len = sb->p.pos = (strbsize_t)len;
#if STRB_UNPUTC
        assert(!(sb->p.flags & F_OVERWRITE)); // needn't set unputc_char
        if (len)
            sb->p.flags |= F_CAN_UNPUTC;
#endif
        return sb;
    }
}

_Optional strb_t *strb_dup(const char *str)
{
        return strb_ndup(str, SIZE_MAX);
}

_Optional strb_t *strb_aprintf(const char *restrict format,
                                ...)
{
    va_list args;
    va_start(args, format);
    {
        strb_t *sb = strb_vaprintf(format, args);
        va_end(args);
        return sb;
    }
}

void strb_free(_Optional strb_t *sb)
{
    if (!sb)
        return;

    if (sb->p.flags & F_AUTOFREE)
        return;

#if !STRB_STATIC_ALLOC
    if (sb->p.flags & F_ALLOCATED)
        free(sb->p.buf);
#endif

    free_metadata(sb);
}
#endif // !STRB_FREESTANDING

#undef strb_ptr
char *strb_ptr(strb_t *sb)
{
    assert(sb);
    return sb->p.buf;
}

const char *strb_cptr(strb_t const *sb)
{
    assert(sb);
    return sb->p.buf;
}

size_t strb_len(strb_t const *sb )
{
        assert(sb);
        return sb->p.len;
}

static int set_err(strb_t *sb)
{
        sb->p.flags |= F_ERR;
        return EOF;
}

int strb_setmode(strb_t *sb, int mode)
{
    assert(sb);
    if (mode == strb_insert || mode == strb_overwrite)
    {
        sb->p.flags &= ~(F_CAN_UNPUTC|F_OVERWRITE);
        if (mode == strb_overwrite)
            sb->p.flags |= F_OVERWRITE;
        return 0;
    } else {
        DEBUGF("Bad mode %d\n", mode);
        return set_err(sb);
    }
}

int strb_getmode(const strb_t *sb )
{
    assert(sb);
    {
        int mode = (sb->p.flags & F_OVERWRITE) ? strb_overwrite : strb_insert;
        assert(mode == strb_insert || mode == strb_overwrite);
        return mode;
    }
}

int strb_seek(strb_t *sb, size_t pos)
{
    assert(sb);
    DEBUGF("Seek to %zu\n", pos);
    if (pos < STRB_MAX_SIZE)
    {
        sb->p.pos = pos;
#if STRB_UNPUTC || STRB_RESTORE
        sb->p.flags &= ~(F_CAN_UNPUTC | F_CAN_RESTORE);
#endif
        return 0;
    } else {
        DEBUGF("Bad seek %zu\n", pos);
        return set_err(sb);
    }
}

size_t strb_tell(strb_t const *sb )
{
    assert(sb);
    {
        strbsize_t pos = sb->p.pos;
        DEBUGF("Pos %" PRIstrbsize ", len %" PRIstrbsize ", size %" PRIstrbsize "\n",
               pos, sb->p.len, sb->p.size);
        return pos;
    }
}

int strb_putc(strb_t *sb, int c)
{
        return strb_nputc(sb, c, 1);
}

int strb_nputc(strb_t *sb, int c, size_t n)
{
    _Optional char *buf = strb_write(sb, n);
    if (!buf)
        return EOF;

    memset(buf, c, n);
    // assume F_CAN_RESTORE isn't user-visible. Don't bother calling strb_restore.
    return c;
}

#if STRB_UNPUTC
int strb_unputc(strb_t *sb)
{
    assert(sb);
    if (!(sb->p.flags & F_CAN_UNPUTC))
        return set_err(sb);
    
    assert(sb->p.pos > 0);
    assert(sb->p.pos < STRB_MAX_SIZE);
    assert(sb->p.pos <= sb->p.len);

    {
        const strbsize_t new_pos = sb->p.pos - 1;
        char removed = sb->p.buf[new_pos];
        if (!(sb->p.flags & F_OVERWRITE)) {
                memmove(sb->p.buf + new_pos, sb->p.buf + sb->p.pos, sb->p.len - new_pos);
                --sb->p.len;
        } else {
                sb->p.buf[new_pos] = sb->p.unputc_char;
        }

        sb->p.pos = new_pos;
#if STRB_UNPUTC || STRB_RESTORE
        sb->p.flags &= ~(F_CAN_UNPUTC | F_CAN_RESTORE);
#endif
        return removed;
    }
}
#endif

int strb_nputs(strb_t *restrict sb, const char *restrict str, size_t n)
{
    size_t len = strnlen(str, n);
    _Optional char *buf = strb_write(sb, len);
    if (!buf)
            return EOF;

    memcpy(buf, str, len); // more efficient than strncpy
    // assume F_CAN_RESTORE isn't user-visible. Don't bother calling strb_restore.
    return 0;
}

int strb_puts(strb_t *sb, const char *str )
{
    return strb_nputs(sb, str, SIZE_MAX);
}

#if !STRB_FREESTANDING

int strb_vputf(strb_t *restrict sb, const char *restrict format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    {
        const int len = vsnprintf(NULL, 0, format, args);
        if (len >= 0) {
            _Optional char *buf = strb_write(sb, (size_t)len); // move tail by +len and keep buf[len]
            if (buf) {
                int const tmp = buf[len];
                vsprintf(buf, format, args_copy);
                buf[len] = tmp;
                DEBUGF("String is now %s\n", strb_ptr(sb));
                va_end(args_copy);
                return 0;
            }
        }
    }
    va_end(args_copy);
    return set_err(sb);
}

int strb_putf(strb_t *restrict sb, const char *restrict format, ...)
{
    va_list args;
    va_start(args, format);
    {
        int e = strb_vputf(sb, format, args);
        va_end(args);
        return e;
    }
}

#endif // !STRB_FREESTANDING

static bool strb_ensure(strb_t *sb, size_t n, strbsize_t top)
{
    assert(sb);

    if (n >= (size_t)STRB_MAX_SIZE - top) {
        DEBUGF("Integer range exhausted (top=%" PRIstrbsize ", n=%zu)\n", top, n);
        return false; // can't represent new length
    }

    {
        const strbsize_t room = sb->p.size > top ? sb->p.size - top : 0;
        DEBUGF("Need %zu bytes, have %" PRIstrbsize " bytes\n", n, room);
        if (n < room)
            return true; // enough room for n chars and terminator
    }

#if STRB_STATIC_ALLOC || STRB_FREESTANDING
    DEBUGF("Fixed buffer exhausted\n");
    return false;
#else
    if (sb->p.flags & F_EXTERNAL) {
        DEBUGF("External buffer exhausted\n");
        return false;
    }

    strbsize_t new_size = sb->p.size <= (STRB_MAX_SIZE / STRB_GROW_FACTOR) ?
                              sb->p.size * STRB_GROW_FACTOR :
                              STRB_MAX_SIZE;

    assert(top + n + 1 <= STRB_MAX_SIZE);

    if (new_size <= top + n)
        new_size = top + n + 1; // +1 for terminator

    char *new_buf = NULL;
    if (sb->p.flags & F_ALLOCATED) {
        new_buf = realloc(sb->p.buf, new_size);
        if (!new_buf)
            return false;
    } else {
        new_buf = malloc(new_size);
        if (!new_buf)
            return false;
        memcpy(new_buf, sb->internal, sb->p.len + 1);
    }

    sb->p.flags |= F_ALLOCATED;
    sb->p.buf = new_buf;
    sb->p.size = new_size;
    DEBUGF("Substituted buffer %p of %" PRIstrbsize " bytes\n", new_buf, new_size);
    return true;
#endif
}

_Optional char *strb_write(strb_t *sb, size_t n)
{
    assert(sb);
    assert(sb->p.len < sb->p.size);
    assert(sb->p.buf[sb->p.len] == '\0');
    DEBUGF("About to write %zu chars\n", n);

    {
        const strbsize_t old_len = sb->p.len, old_pos = sb->p.pos;
        const strbsize_t top = (sb->p.flags & F_OVERWRITE) || old_pos > old_len ?
                               old_pos : old_len;

        if (!strb_ensure(sb, n, top)) {
            DEBUGF("No room\n");
            set_err(sb);
            return NULL;
        }

        assert(old_pos < sb->p.size);

        if (old_pos > old_len) {
            DEBUGF("Zeroing between len %" PRIstrbsize " and pos %" PRIstrbsize "\n", old_len, old_pos + 1);
            // +1 because there is no null terminator at pos yet
            memset(sb->p.buf + old_len, '\0', old_pos - old_len + 1);
            sb->p.len = old_pos;
        }

        assert(old_pos <= sb->p.len);

        {
            _Optional char *buf = sb->p.buf + old_pos;

            if (!(sb->p.flags & F_OVERWRITE)) {
                DEBUGF("Moving tail '%s' (%d) from %p to %p\n", buf, *buf, buf, buf + n);
                memmove(buf + n, buf, sb->p.len + 1 - old_pos);
                sb->p.len += n;
            } else {
#if STRB_UNPUTC
                // Behave as if the write were implemented by multiple
                // putc operations, which would imply null termination at
                // each successive position.
                sb->p.unputc_char = old_pos + n - 1 > old_len ? '\0' : buf[n - 1];
#endif
            }

            sb->p.pos = old_pos + n;
            DEBUGF("Pos advanced by %zu to %" PRIstrbsize "\n", n, sb->p.pos);
            assert(sb->p.pos < sb->p.size);

            if (sb->p.pos > sb->p.len) {
                DEBUGF("Bumping length from %" PRIstrbsize " to %" PRIstrbsize "\n", sb->p.len, sb->p.pos);
                sb->p.len = sb->p.pos;
                buf[n] = '\0';
            }

#if STRB_RESTORE
            sb->p.restore_char = buf[n];
            sb->p.flags |= F_CAN_RESTORE;
            DEBUGF("Stored %d ('%c') at %" PRIstrbsize "\n", sb->p.restore_char, sb->p.restore_char, sb->p.pos);
#endif

#if STRB_UNPUTC
            if (n)
                sb->p.flags |= F_CAN_UNPUTC;
#endif
            return buf;
        }
    }
}

void strb_split(strb_t *sb)
{
    _Optional char *p = strb_write(sb, 0);
    assert(p);
    *(char *)p = '\0';
}

#if STRB_RESTORE
void strb_restore(strb_t *sb)
{
    assert(sb);
    if (sb->p.flags & F_CAN_RESTORE) {
        DEBUGF("Restored %d ('%c') at %" PRIstrbsize "\n", sb->p.restore_char, sb->p.restore_char, sb->p.pos);
        sb->p.buf[sb->p.pos] = sb->p.restore_char;
        sb->p.flags &= ~F_CAN_RESTORE;
    }
}
#endif

void strb_delto(strb_t *sb, size_t pos)
{
    strbsize_t hi, lo, pos1, pos2, len;

    assert(sb);
    assert(sb->p.pos <= sb->p.len);

    len = sb->p.len;
    pos1 = pos > len ? len : pos;
    pos2 = sb->p.pos > len ? len : sb->p.pos;

    if (pos1 > pos2) {
            hi = pos1;
            lo = pos2;
    } else {
            lo = pos1;
            hi = pos2;
    }

    if (!(sb->p.flags & F_OVERWRITE)) {
            memmove(sb->p.buf + lo, sb->p.buf + hi, len + 1 - hi);
            sb->p.len = len - (hi - lo);
    }

    sb->p.pos = lo;
#if STRB_UNPUTC || STRB_RESTORE
    sb->p.flags &= ~(F_CAN_UNPUTC | F_CAN_RESTORE);
#endif
}

static void strb_empty(strb_t *sb)
{
    assert(sb);
    sb->p.len = sb->p.pos = 0;
    sb->p.buf[0] = '\0';
#if STRB_UNPUTC || STRB_RESTORE
    sb->p.flags &= ~(F_CAN_UNPUTC | F_CAN_RESTORE);
#endif
}

int strb_ncpy(strb_t *restrict sb, const char *restrict str, size_t n)
{
    strb_empty(sb);
    return strb_nputs(sb, str, n);
}

int strb_cpy(strb_t *restrict sb,
             const char *restrict str )
{
    return strb_ncpy(sb, str, SIZE_MAX);
}

#if !STRB_FREESTANDING

int strb_vprintf(strb_t *restrict sb, const char *restrict format, va_list args)
{
    strb_empty(sb);
    return strb_vputf(sb, format, args);
}

int strb_printf(strb_t *restrict sb, const char *restrict format, ...)
{
    va_list args;
    va_start(args, format);
    {
        int e = strb_vprintf(sb, format, args);
        va_end(args);
        return e;
    }
}

#endif // !STRB_FREESTANDING

bool strb_error(strb_t const *sb )
{
    assert(sb);
    return (sb->p.flags & F_ERR) != 0;
}

void strb_clearerr(strb_t *sb)
{
    assert(sb);
    sb->p.flags &= ~F_ERR;
}
