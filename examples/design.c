// Copyright 2026 Christopher Bazley
// SPDX-License-Identifier: MIT

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strb.h"

//! [producer]
static int put_label(strb_t *sb, unsigned number)
{
    assert(number < 10);
    return strb_putf(sb, "item%u", number);
}
//! [producer]

static bool editing_example(void)
{
//! [external]
    char text[128];
    strbstate_t state;
    strb_t *sb = strb_use(&state, sizeof text, text);
//! [external]

//! [sequential]
    strb_puts(sb, "Selected: ");
    put_label(sb, 7);
    if (strb_error(sb))
        return false;
    // "Selected: item7"; position 15, insert mode.
//! [sequential]
    if (strcmp(strb_ptr(sb), "Selected: item7"))
        return false;

//! [append]
    if (strb_seek(sb, strb_len(sb)) || strb_puts(sb, "."))
        return false;
    // "Selected: item7."; position 16.
//! [append]
    if (strcmp(strb_ptr(sb), "Selected: item7."))
        return false;

//! [prepend]
    if (strb_setmode(sb, strb_insert) || strb_seek(sb, 0) ||
        strb_puts(sb, "New "))
        return false;
    const size_t label_start = strb_tell(sb);
    // "New Selected: item7."; position 4, insert mode.
//! [prepend]
    if (strcmp(strb_ptr(sb), "New Selected: item7."))
        return false;

//! [insert]
    strb_setmode(sb, strb_insert);
    strb_seek(sb, label_start);
    put_label(sb, 2);
    strb_puts(sb, ": ");
    if (strb_error(sb))
        return false;
    const size_t label_end = strb_tell(sb);
    // "New item2: Selected: item7."; position 11, insert mode.
//! [insert]
    if (strcmp(strb_ptr(sb), "New item2: Selected: item7."))
        return false;

//! [overwrite]
    if (strb_setmode(sb, strb_overwrite) || strb_seek(sb, label_start) ||
        put_label(sb, 3))
        return false;
    // "New item3: Selected: item7."; position 9, overwrite mode.
//! [overwrite]
    if (strcmp(strb_ptr(sb), "New item3: Selected: item7."))
        return false;

//! [delete]
    strb_setmode(sb, strb_insert);
    strb_seek(sb, label_start);
    strb_delto(sb, label_end);
    // "New Selected: item7."; position 4, insert mode.
//! [delete]
    return strb_tell(sb) == label_start &&
           !strcmp(strb_ptr(sb), "New Selected: item7.");
}

//! [fruit_producer]
static void put_fruit_list(strb_t *sb, size_t count,
                           const unsigned indices[static count])
{
    const char *const fruit[] = {"apple", "orange", "banana", "lime"};
    for (size_t i = 0; i < count; ++i) {
        if (i)
            strb_putc(sb, ',');
        strb_puts(sb, fruit[indices[i]]);
    }
}
//! [fruit_producer]

//! [fruit_sentence]
static bool fruit_sentence_example(void)
{
    const unsigned indices[] = {0, 2, 1};
    char text[64];
    strbstate_t state;
    strb_t *sb = strb_use(&state, sizeof text, text);

    strb_puts(sb, "Fruit: ");
    const size_t list_position = strb_tell(sb);
    strb_putc(sb, '.');
    strb_seek(sb, list_position);
    put_fruit_list(sb, sizeof indices / sizeof indices[0], indices);
    if (strb_error(sb))
        return false;
    return puts(text) != EOF;
}
//! [fruit_sentence]

//! [fruit_caller]
static bool fruit_rows_example(void)
{
    const unsigned rows[][3] = {{0, 2, 1}, {3, 1, 0}};
    _Optional strb_t *sb = strb_alloc(0);
    if (!sb)
        return false;

    bool success = true;
    for (size_t row = 0; row < sizeof rows / sizeof rows[0]; ++row) {
        // Replace the previous result; retain the allocated storage.
        strb_cpy(&*sb, "");
        put_fruit_list(&*sb, sizeof rows[row] / sizeof rows[row][0], rows[row]);
        if (strb_error(&*sb)) {
            success = false;
            break;
        }
        if (puts(strb_ptr(&*sb)) == EOF) {
            success = false;
            break;
        }
    }
    strb_free(sb);
    return success;
}
//! [fruit_caller]

int main(void)
{
    return editing_example() && fruit_sentence_example() && fruit_rows_example()
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
