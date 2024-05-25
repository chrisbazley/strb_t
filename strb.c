
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

#define F_CAN_RESTORE (1<<0)
#define F_ERR (1<<1)
#define F_WRITE_PENDING (1<<2)
#define F_OVERWRITE (1<<3)
#if !TINY && !TINIER
#define F_ALLOCATED (1<<4)
#endif
#define F_EXTERNAL (1<<5)
#define F_AUTOFREE (1<<6)

struct strb_t {
    strbprivate_t p;
#if TINIER
    // No internal storage
#elif TINY
    char internal[STRB_MAX_SIZE];
#else
    char internal[];
#endif
};

#if TINY
static strb_t bufs[STRB_MAX];
static uint8_t nbufs, buf_map;
#endif

#if TINY || TINIER
// not provided by cc65
size_t strnlen(const char *s, size_t n)
{
    size_t p = 0;
    while (p < n && s[p])
        p++;
    return p;
}
#endif

#if TINY
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
#elif !TINIER
static _Optional strb_t *alloc_metadata(strbsize_t size)
{
    assert(size <= STRB_MAX_INTERNAL_SIZE);
    return malloc(sizeof(strb_t) + size);
}

static void free_metadata(_Optional strb_t *sb)
{
    free(sb);
}
#endif

static strb_t *init_use(strbstate_t *sb, strbsize_t size, char buf[STRB_SIZE_HINT(size)], strbsize_t len)
{
    sb->p.len = sb->p.pos = len;
    sb->p.size = size;
    sb->p.buf = buf;
    sb->p.flags = F_EXTERNAL | F_AUTOFREE;
    return (void *)sb;
}

strb_t *strbstate_use(strbstate_t *sb, size_t size, char buf[STRB_SIZE_HINT(size)])
{
    assert(sb);
    assert(buf);
    assert(size > 0);
    DEBUGF("Use buffer %p of size %zu\n", buf, size);

    if (size > STRB_MAX_SIZE)
        size = STRB_MAX_SIZE; // unsupported buffer size

    buf[0] = '\0';
    return init_use(sb, size, buf, 0);
}

_Optional strb_t *strbstate_reuse(strbstate_t *sb, size_t size, char buf[STRB_SIZE_HINT(size)])
{
    assert(sb);
    assert(buf);
    assert(size > 0);
    DEBUGF("Reuse buffer %p of size %zu\n", buf, size);

    if (size > STRB_MAX_SIZE)
        size = STRB_MAX_SIZE; // unsupported buffer size

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

#if !TINIER
_Optional strb_t *strb_use(size_t size, char buf[STRB_SIZE_HINT(size)])
{
    assert(buf);
    assert(size > 0);
    DEBUGF("Use buffer %p of size %zu\n", buf, size);

    if (size > STRB_MAX_SIZE)
        size = STRB_MAX_SIZE; // unsupported buffer size

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
        size = STRB_MAX_SIZE; // unsupported buffer size

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
    }
    sb->p.flags = F_EXTERNAL;
    return sb;
}

_Optional strb_t *strb_alloc(size_t size)
{
    DEBUGF("Alloc buffer of size %zu\n", size);

#if TINY
    size = STRB_MAX_INTERNAL_SIZE;
#else
    if (size > STRB_MAX_SIZE || size < STRB_DFL_SIZE)
        size = STRB_DFL_SIZE;
#endif
    {
        // Don't allocate huge internal strings because the storage can't be recovered
        _Optional strb_t *sb = alloc_metadata(
            size > STRB_MAX_INTERNAL_SIZE ? 0 : size);
        if (!sb) return NULL;
#if !TINY
        if (size > STRB_MAX_INTERNAL_SIZE) {
            DEBUGF("Oversize buffer of %zu bytes\n", size);
            sb->p.buf = malloc(size);
            if (!sb->p.buf) {
                free_metadata(sb);
                return NULL;
            }
            sb->p.flags = F_ALLOCATED;
        } else
#endif
        {
            DEBUGF("Internal buffer of %zu bytes\n", size);
            sb->p.buf = sb->internal;
            sb->p.flags = 0;
        }

        sb->p.len = sb->p.pos = 0;
        sb->p.size = size;
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
        sb->p.flags |= F_CAN_RESTORE;
        return sb;
    }
}

_Optional strb_t *strb_vasprintf(const char *format, va_list args)
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
        sb->p.flags |= F_CAN_RESTORE;
        return sb;
    }
}

_Optional strb_t *strb_dup(const char *str)
{
        return strb_ndup(str, SIZE_MAX);
}

_Optional strb_t *strb_asprintf(const char *format,
                                ...)
{
    va_list args;
    va_start(args, format);
    {
        strb_t *sb = strb_vasprintf(format, args);
        va_end(args);
        return sb;
    }
}

#endif

#if !TINIER
void strb_free(_Optional strb_t *sb)
{
    if (!sb)
        return;

    if (sb->p.flags & F_AUTOFREE)
        return;

#if !TINY
    if (sb->p.flags & F_ALLOCATED)
        free(sb->p.buf);
#endif

    free_metadata(sb);
}
#endif

const char *strb_ptr(strb_t const *sb )
{
    assert(sb);
    return sb->p.buf;
}

size_t strb_len(strb_t const *sb )
{
        assert(sb);
        return sb->p.len;
}

int strb_setmode(strb_t *sb, int mode)
{
    assert(sb);
    assert(mode == strb_insert || mode == strb_overwrite);
    {
        int old_mode = (sb->p.flags & F_OVERWRITE) ? strb_overwrite : strb_insert;
        sb->p.flags &= ~(F_CAN_RESTORE|F_OVERWRITE);
        if (mode == strb_overwrite)
            sb->p.flags |= F_OVERWRITE;
        return old_mode;
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

size_t strb_seek(strb_t *sb, size_t pos)
{
    assert(sb);
    DEBUGF("Seek to %zu\n", pos);
    {
        size_t old_pos = sb->p.pos;
        sb->p.pos = pos;
        sb->p.flags &= ~(F_CAN_RESTORE | F_WRITE_PENDING);
        return old_pos;
    }
}

size_t strb_tell(strb_t const *sb )
{
    assert(sb);
    {
        size_t pos = sb->p.pos;
        DEBUGF("Pos %zu, len %" PRIstrbsize ", size %" PRIstrbsize "\n", pos, sb->p.len, sb->p.size);
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
    // assume F_WRITE_PENDING isn't user-visible. Don't bother calling strb_wrote.
    return 0;
}

int strb_unputc(strb_t *sb)
{
    assert(sb);
    if (!(sb->p.flags & F_CAN_RESTORE)) {
        sb->p.flags |= F_ERR;
        return EOF;
    }
    
    assert(sb->p.pos > 0);
    assert(sb->p.pos < STRB_MAX_SIZE);
    assert(sb->p.pos <= sb->p.len);

    {
        strbsize_t pos = sb->p.pos;
        char removed = sb->p.buf[pos - 1];
        if (!(sb->p.flags & F_OVERWRITE)) {
                memmove(sb->p.buf + pos - 1, sb->p.buf + pos, sb->p.len + 1 - pos);
                --sb->p.len;
        } else {
                sb->p.buf[pos - 1] = sb->p.restore_char;
        }

        sb->p.pos = pos - 1;
        sb->p.flags &= ~(F_CAN_RESTORE | F_WRITE_PENDING);
        return removed;
    }
}

int strb_nputs(strb_t *sb, const char *str, size_t n)
{
    size_t len = strnlen(str, n);
    _Optional char *buf = strb_write(sb, len);
    if (!buf)
            return EOF;

    memcpy(buf, str, len); // more efficient than strncpy
    // assume F_WRITE_PENDING isn't user-visible. Don't bother calling strb_wrote.
    return 0;
}

int strb_puts(strb_t *sb, const char *str )
{
    return strb_nputs(sb, str, SIZE_MAX);
}

int strb_vputf(strb_t *sb, const char *format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    {
        int len = vsnprintf(NULL, 0, format, args);
        if (len >= 0) {
            _Optional char *buf = strb_write(sb, (size_t)len); // move tail by +len and keep buf[len]
            if (buf) {
                    vsprintf(buf, format, args_copy);
                    strb_wrote(sb); // restore buf[len] overwritten by null
                    DEBUGF("String is now %s\n", strb_ptr(sb));
            } else {
                    len = EOF;
            }
        } else {
            sb->p.flags |= F_ERR;
        }

        va_end(args_copy);
        return len;
    }
}

int strb_putf(strb_t *sb, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    {
        int len = strb_vputf(sb, format, args);
        va_end(args);
        return len;
    }
}

static bool strb_ensure(strb_t *sb, size_t n, strbsize_t top)
{
    assert(sb);

    if (n >= (size_t)STRB_MAX_SIZE - top) {
        DEBUGF("Integer range exhausted (top=%" PRIstrbsize ", n=%zu)\n", top, n);
        return false; // can't represent new length
    }

    {
        strbsize_t room = sb->p.size > top ? sb->p.size - top : 0;
        DEBUGF("Need %zu bytes, have %" PRIstrbsize " bytes\n", n, room);
        if (n < room)
            return true; // enough room for n chars and terminator
    }

#if TINY || TINIER
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
        bool outside = sb->p.pos > sb->p.len;
        size_t top = (sb->p.flags & F_OVERWRITE) || outside ?
                            sb->p.pos : sb->p.len;
        if (top > STRB_MAX_SIZE) {
            DEBUGF("Integer range exhausted (top=%zu)\n", top);
            return false; // can't represent new length
        }

        if (!strb_ensure(sb, n, top)) {
            DEBUGF("No room\n");
            sb->p.flags |= F_ERR;
            return NULL;
        }

        assert(sb->p.pos < sb->p.size);

        if (outside) {
            DEBUGF("Zeroing gap between len %" PRIstrbsize " and pos %zu\n", sb->p.len, sb->p.pos);
            // +1 because there is no null terminator at pos yet
            memset(sb->p.buf + sb->p.len, '\0', sb->p.pos - sb->p.len + 1);
            sb->p.len = sb->p.pos;
        }
    }

    assert(sb->p.pos <= sb->p.len);

    {
        _Optional char *buf = sb->p.buf + sb->p.pos;
        if (!(sb->p.flags & F_OVERWRITE)) {
            DEBUGF("Moving tail '%s' (%d) from %p to %p\n", buf, *buf, buf, buf + n);
            memmove(buf + n, buf, sb->p.len + 1 - sb->p.pos);
            sb->p.len += n;
        } else
            sb->p.restore_char = buf[n - 1];

        sb->p.pos += n;
        DEBUGF("Pos advanced by %zu to %zu\n", n, sb->p.pos);
        assert(sb->p.pos < sb->p.size);

        if (sb->p.pos > sb->p.len) {
            DEBUGF("Bumping length from %" PRIstrbsize " to %zu\n", sb->p.len, sb->p.pos);
            sb->p.len = sb->p.pos;
            buf[n] = '\0';
        }

        sb->p.write_char = buf[n];
        sb->p.flags |= F_CAN_RESTORE | F_WRITE_PENDING;
        DEBUGF("Stored %d ('%c') at %zu\n", sb->p.write_char, sb->p.write_char, sb->p.pos);
        return buf;
    }
}

void strb_wrote(strb_t *sb)
{
    assert(sb);
    if (sb->p.flags & F_WRITE_PENDING) {
        DEBUGF("Restored %d ('%c') at %zu\n", sb->p.write_char, sb->p.write_char, sb->p.pos);
        sb->p.buf[sb->p.pos] = sb->p.write_char;
        sb->p.flags &= ~F_WRITE_PENDING;
    }
}

void strb_delto(strb_t *sb, size_t pos)
{
    size_t hi, lo;

    if (pos > sb->p.pos) {
            hi = pos;
            lo = sb->p.pos;
    } else {
            lo = pos;
            hi = sb->p.pos;
    }
    if (hi > sb->p.len) hi = sb->p.len;
    if (lo > sb->p.len) lo = sb->p.len;

    if (!(sb->p.flags & F_OVERWRITE)) {
            memmove(sb->p.buf + lo, sb->p.buf + hi, sb->p.len + 1 - hi);
            sb->p.len -= hi - lo;
    }

    sb->p.pos = lo;
    sb->p.flags &= ~(F_CAN_RESTORE | F_WRITE_PENDING);

}

static void strb_empty(strb_t *sb)
{
    assert(sb);
    sb->p.len = sb->p.pos = 0;
    sb->p.buf[0] = '\0';
    sb->p.flags &= ~(F_CAN_RESTORE | F_WRITE_PENDING);
}

int strb_ncpy(strb_t *sb, const char *str, size_t n)
{
    strb_empty(sb);
    return strb_nputs(sb, str, n);
}

int strb_cpy(strb_t *sb,
             const char *str )
{
    return strb_ncpy(sb, str, SIZE_MAX);
}

int strb_vprintf(strb_t *sb, const char *format, va_list args)
{
    strb_empty(sb);
    return strb_vputf(sb, format, args);
}

int strb_printf(strb_t *sb, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    {
        int len = strb_vprintf(sb, format, args);
        va_end(args);
        return len;
    }
}

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
