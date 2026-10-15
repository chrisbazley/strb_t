// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

#define vnprintf(fmt, ap) vsnprintf(NULL, 0, fmt, ap)

_Optional strb_t *strb_vaprintf(const char *STRB_CONST restrict format, va_list args)
{
    _Optional strb_t *sb = NULL;
    va_list args_copy;
    va_copy(args_copy, args);

    do {
        const int len = vnprintf(format, args);

        if (len < 0 || (size_t)len >= STRB_MAX_SIZE) {
            break; // formatting failed or result too long
        }

        sb = strb_alloc((size_t)len + 1u);

        if (!sb) {
            break;
        }

        assert(sb->p.size > len);
        vsprintf(sb->p.buf, format, args_copy);

        sb->p.len = sb->p.pos = (strbsize_t)len;
#if STRB_UNPUTC
        assert(!(sb->p.flags & F_OVERWRITE)); // needn't set unputc_char

        if (len) {
            sb->p.flags |= F_CAN_UNPUTC;
        }
#endif
    } while (0);

    va_end(args_copy);
    return sb;
}

_Optional strb_t *strb_aprintf(const char *STRB_CONST restrict format, ...)
{
    va_list args;
    va_start(args, format);
    {
        _Optional strb_t *const sb = strb_vaprintf(format, args);
        va_end(args);
        return sb;
    }
}

int strb_vputf(strb_t *STRB_CONST restrict sb,
    const char *STRB_CONST restrict format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    {
        const int len = vnprintf(format, args);

        if (len >= 0) {
            _Optional char *const buf = strb_write(
                sb, (size_t)len); // move tail by +len and keep buf[len]

            if (buf) {
                char const tmp = buf[len];
                vsnprintf(buf, (size_t)len + 1u, format, args_copy);
                buf[len] = tmp;
                DEBUGF("String is now %s\n", strb_ptr(sb));
                va_end(args_copy);
                return 0;
            }
        }
    }
    va_end(args_copy);
    return strb_set_error(sb);
}

int strb_putf(strb_t *STRB_CONST restrict sb, const char *STRB_CONST restrict format, ...)
{
    va_list args;
    va_start(args, format);
    {
        const int e = strb_vputf(sb, format, args);
        va_end(args);
        return e;
    }
}

int strb_vprintf(strb_t *STRB_CONST restrict sb,
    const char *STRB_CONST restrict format, va_list args)
{
    strb_empty(sb);
    return strb_vputf(sb, format, args);
}

int strb_printf(strb_t *STRB_CONST restrict sb, const char *STRB_CONST restrict format, ...)
{
    va_list args;
    va_start(args, format);
    {
        const int e = strb_vprintf(sb, format, args);
        va_end(args);
        return e;
    }
}
