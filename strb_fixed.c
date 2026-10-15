// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#include "strb_internal.h"

bool strb_grow(strb_t *STRB_CONST sb, STRB_CONST strbsize_t minimum_size)
{
    (void)sb;
    (void)minimum_size;
    DEBUGF("Fixed buffer exhausted\n");
    return false;
}
