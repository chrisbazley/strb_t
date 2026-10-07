// Copyright 2026 Christopher Bazley
// SPDX-License-Identifier: MIT

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <uchar.h>
#include <stdlib.h>
#include <string.h>
#include "strb.h"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

//! [producer]
static int put_path_segment(strb_t *sb, const char *segment)
{
    return strb_putf(sb, "/%s", segment);
}
//! [producer]

static bool editing_example(void)
{
//! [external]
    char text[64];
    strbstate_t state;
    strb_t *const sb = strb_use(&state, sizeof text, text);
//! [external]

//! [sequential]
    put_path_segment(sb, "manual");
    put_path_segment(sb, "guide.html");
    if (strb_error(sb))
        return false;
    // "/manual/guide.html"; position at the end, insert mode.
//! [sequential]
    if (strcmp(strb_ptr(sb), "/manual/guide.html"))
        return false;

//! [append]
    strb_seek(sb, strb_len(sb));
    if (strb_puts(sb, "#details"))
        return false;
    // "/manual/guide.html#details"; position at the end.
//! [append]
    if (strcmp(strb_ptr(sb), "/manual/guide.html#details"))
        return false;

//! [prepend]
    strb_setmode(sb, strb_insert);
    strb_seek(sb, 0);
    if (strb_puts(sb, "https://example.org"))
        return false;
    const size_t path_start = strb_tell(sb);
    // "https://example.org/manual/guide.html#details"; position before the path.
//! [prepend]
    if (strcmp(strb_ptr(sb), "https://example.org/manual/guide.html#details"))
        return false;

//! [insert]
    if (put_path_segment(sb, "docs"))
        return false;
    // "https://example.org/docs/manual/guide.html#details".
//! [insert]
    if (strcmp(strb_ptr(sb), "https://example.org/docs/manual/guide.html#details"))
        return false;

//! [overwrite]
    strb_setmode(sb, strb_overwrite);
    strb_seek(sb, path_start);
    if (put_path_segment(sb, "help"))
        return false;
    // "https://example.org/help/manual/guide.html#details"; overwrite mode.
//! [overwrite]
    if (strcmp(strb_ptr(sb), "https://example.org/help/manual/guide.html#details"))
        return false;

//! [delete]
    strb_setmode(sb, strb_insert);
    strb_delto(sb, path_start);
    // "https://example.org/manual/guide.html#details"; position before the path.
//! [delete]
    return strb_tell(sb) == path_start &&
           !strcmp(strb_ptr(sb), "https://example.org/manual/guide.html#details");
}

//! [fruit_producer]
static void put_fruit_list(strb_t *sb, size_t count,
                           const unsigned indices[STRB_SIZE_HINT(count)])
{
    const char *const fruit[] = {"apple", "orange", "banana", "lime"};
    for (size_t i = 0; i < count; ++i) {
        if (i)
            strb_putc(sb, ',');
        strb_puts(sb, indices[i] < ARRAY_SIZE(fruit) ? fruit[indices[i]] : "unknown");
    }
}
//! [fruit_producer]

//! [fruit_sentence]
static bool fruit_sentence_example(void)
{
    const unsigned indices[] = {0, 2, 1};
    char text[64];
    strbstate_t state;
    strb_t *const sb = strb_use(&state, sizeof text, text);

    strb_puts(sb, "Fruit: ");
    const size_t list_position = strb_tell(sb);
    strb_putc(sb, '.');
    strb_seek(sb, list_position);
    put_fruit_list(sb, ARRAY_SIZE(indices), indices);
    if (strb_error(sb))
        return false;
    return puts(text) != EOF;
}
//! [fruit_sentence]

//! [fruit_caller]
static bool fruit_rows_example(void)
{
    const unsigned rows[][3] = {{0, 2, 1}, {3, 1, 0}};
    _Optional strb_t *const sb = strb_alloc(0);
    if (!sb)
        return false;

    bool success = true;
    for (size_t row = 0; row < ARRAY_SIZE(rows); ++row) {
        // Replace the previous result; retain the allocated storage.
        strb_cpy(&*sb, "");
        put_fruit_list(&*sb, ARRAY_SIZE(rows[row]), rows[row]);
        if (strb_error(&*sb) || puts(strb_ptr(&*sb)) == EOF) {
            success = false;
            break;
        }
    }
    strb_free(sb);
    return success;
}
//! [fruit_caller]

//! [short_write]
static bool put_character(strb_t *sb, char32_t character)
{
    assert(character != 0);
    mbstate_t conversion = {0};
    const size_t start = strb_tell(sb);
    _Optional char *const output = strb_write(sb, MB_LEN_MAX);
    if (!output)
        return false;

    const size_t count = c32rtomb(&*output, character, &conversion);
    if (count == (size_t)-1) {
        strb_delto(sb, start);
        return false;
    }
    strb_delto(sb, start + count);
    return true;
}
//! [short_write]

static bool short_write_example(void)
{
    const char original[] = "012345678901234567890123456789";
    const int modes[] = {strb_insert, strb_overwrite};
    char text[64];
    strbstate_t state;
    strb_t *const sb = strb_use(&state, sizeof text, text);

    for (size_t i = 0; i < ARRAY_SIZE(modes); ++i) {
        if (strb_cpy(sb, original) || strb_setmode(sb, modes[i]))
            return false;
        strb_seek(sb, 0);
        if (!put_character(sb, U'A'))
            return false;
        if (strb_tell(sb) != 1)
            return false;
        if (modes[i] == strb_insert) {
            if (strb_len(sb) != sizeof original ||
                strcmp(strb_cptr(sb), "A012345678901234567890123456789"))
                return false;
        } else {
            if (strb_len(sb) != sizeof original - 1 ||
                strcmp(strb_cptr(sb), "A12345678901234567890123456789"))
                return false;
        }
    }
    return true;
}

int main(void)
{
    return editing_example() && fruit_sentence_example() && fruit_rows_example() &&
                   short_write_example()
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
