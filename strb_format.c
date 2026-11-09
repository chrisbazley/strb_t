// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include "strb_internal.h"

#if STRB_FREESTANDING
#error "Do not compile strb_format.c with STRB_FREESTANDING"
#endif

#define vnprintf(fmt, ap) vsnprintf(NULL, 0, fmt, ap)

_Optional strb_t *strb_vaprintf(const char *STRB__CONST restrict format, va_list args)
{
    _Optional strb_t *const sb = strb_alloc(0);

    if (sb && strb_vprintf(&*sb, format, args)) {
        strb_destroy(sb);
        return NULL;
    }

    return sb;
}

_Optional strb_t *strb_aprintf(const char *STRB__CONST restrict format, ...)
{
    va_list args;
    va_start(args, format);
    {
        _Optional strb_t *const sb = strb_vaprintf(format, args);
        va_end(args);
        return sb;
    }
}

static int vputf(strb_t *STRB__CONST restrict sb,
    const char *STRB__CONST restrict format, va_list args, STRB__CONST bool replace)
{
    va_list args_copy;
    va_copy(args_copy, args);
    {
        const int len = vnprintf(format, args);

        if (len >= 0) {
            _Optional char *const buf = replace ? strb__replace(sb, (size_t)len)
                : strb_write(sb, (size_t)len);

            if (buf) {
                char const tmp = buf[len];
                const int written = vsnprintf(buf, (size_t)len + 1u, format, args_copy);
                buf[len] = tmp;
                assert(written == len);
                (void)written;
                DEBUGF("String is now %s\n", strb_ptr(sb));
                va_end(args_copy);
                return 0;
            }
        }
    }
    va_end(args_copy);
    return strb__seterr(sb);
}

int strb_vputf(strb_t *STRB__CONST restrict sb,
    const char *STRB__CONST restrict format, va_list args)
{
    return vputf(sb, format, args, false);
}

int strb_putf(strb_t *STRB__CONST restrict sb, const char *STRB__CONST restrict format, ...)
{
    va_list args;
    va_start(args, format);
    {
        const int e = strb_vputf(sb, format, args);
        va_end(args);
        return e;
    }
}

int strb_vprintf(strb_t *STRB__CONST restrict sb,
    const char *STRB__CONST restrict format, va_list args)
{
    return vputf(sb, format, args, true);
}

int strb_printf(strb_t *STRB__CONST restrict sb, const char *STRB__CONST restrict format, ...)
{
    va_list args;
    va_start(args, format);
    {
        const int e = strb_vprintf(sb, format, args);
        va_end(args);
        return e;
    }
}
