// Copyright 2026 Christopher Bazley
// SPDX-License-Identifier: MIT
#include "strb.h"
_Static_assert(_Generic(strb_from((strbstate_t *)0), strb_t *: 1, default: 0),
    "mutable state yields a mutable handle");
_Static_assert(_Generic(strb_from((const strbstate_t *)0), const strb_t *: 1, default: 0),
    "const state yields a const handle");
int main(void)
{
    char a[8], b[8];
    strbstate_t state[2];
    unsigned index = 0;
    const strbstate_t *const cstate = &state[0];
    strb_t *(*const recover)(strbstate_t *) = (strb_from);
    strb_use(&state[0], sizeof a, a);
    strb_use(&state[1], sizeof b, b);
    if (strb_puts(strb_from(&state[index++]), "a") || index != 1) {
        return 1;
    }
    if (strb_len(strb_from(cstate)) != 1 || strb_len(recover(&state[1])) != 0) {
        return 1;
    }
    return 0;
}
