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
static int put_path_segment(strb_t *const sb, const char *const segment)
{
    return strb_putf(sb, "/%s", segment);
}
//! [producer]

static bool test_editing(void)
{
//! [external]
    STRB_DECL_FIXED(sb, 63);
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
    const int old_mode = strb_setmode(sb, strb_overwrite);
    strb_seek(sb, path_start);
    if (put_path_segment(sb, "help"))
        return false;
    // "https://example.org/help/manual/guide.html#details"; overwrite mode.
//! [overwrite]
    if (strcmp(strb_ptr(sb), "https://example.org/help/manual/guide.html#details"))
        return false;

//! [delete]
    strb_setmode(sb, old_mode);
    strb_delto(sb, path_start);
    // "https://example.org/manual/guide.html#details"; position before the path.
//! [delete]
    return strb_tell(sb) == path_start &&
           !strcmp(strb_ptr(sb), "https://example.org/manual/guide.html#details");
}

//! [fruit_producer]
static void put_fruit_list(strb_t *const sb, const size_t count,
                           const unsigned indices[STRB_SIZE_HINT(count)])
{
    static const char *const fruit[] = {"apple", "orange", "banana", "lime"};
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
    static const unsigned indices[] = {0, 2, 1};
    STRB_DECL_FIXED(sb, 63);

    strb_puts(sb, "Fruit: ");
    const size_t list_position = strb_tell(sb);
    strb_putc(sb, '.');
    strb_seek(sb, list_position);
    put_fruit_list(sb, ARRAY_SIZE(indices), indices);
    if (strb_error(sb))
        return false;
    return puts(strb_cptr(sb)) != EOF;
}
//! [fruit_sentence]

//! [fruit_caller]
static bool fruit_rows_example(void)
{
    static const unsigned rows[][3] = {{0, 2, 1}, {3, 1, 0}};
    STRB_DECL_ALLOC(sb, 0);

    bool success = true;
    for (size_t row = 0; row < ARRAY_SIZE(rows); ++row) {
        // Replace the previous result; retain the allocated storage.
        strb_empty(sb);
        put_fruit_list(sb, ARRAY_SIZE(rows[row]), rows[row]);
        if (strb_error(sb) || puts(strb_ptr(sb)) == EOF) {
            success = false;
            break;
        }
    }
    strb_destroy(sb);
    return success;
}
//! [fruit_caller]

//! [fruit_local]
static bool fruit_local_example(void)
{
    static const unsigned indices[] = {0, 2, 1};
    STRB_DECL_ALLOC(sb, 0);

    put_fruit_list(sb, ARRAY_SIZE(indices), indices);
    const bool success = !strb_error(sb) && puts(strb_cptr(sb)) != EOF;
    strb_destroy(sb);
    return success;
}
//! [fruit_local]

//! [short_write]
static bool put_character(strb_t *const sb, const char32_t character)
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

static bool test_short_write(void)
{
    static const char original[] = "012345678901234567890123456789";
    static const int modes[] = {strb_insert, strb_overwrite};
    STRB_DECL_FIXED(sb, 63);

    for (size_t i = 0; i < ARRAY_SIZE(modes); ++i) {
        if (strb_cpy(sb, original) || strb_setmode(sb, modes[i]) == EOF)
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

//! [label_value]
typedef struct {
    int x, y;
    strb_t text;
} label_t;

static void init_label(label_t *label, int x, int y)
{
    *label = (label_t){.x = x, .y = y};
}
//! [label_value]

//! [label_initialisation]
typedef struct {
    int x, y;
    char buffer[32];
    strb_t sb;
} label2_t;

static void init_label2(label2_t *label, int x, int y)
{
    *label = (label2_t){.x = x, .y = y};
    STRB_USE(&label->sb, label->buffer);
}
//! [label_initialisation]

static bool test_label_initialisation(void)
{
    label_t label;
    init_label(&label, 10, 20);
    if (strb_printf(&label.text, "Label at (%d, %d)", label.x, label.y)) {
        strb_destroy(&label.text);
        return false;
    }
    const bool success = !strcmp(strb_cptr(&label.text), "Label at (10, 20)");
    strb_destroy(&label.text);
    if (!success)
        return false;

    label2_t fixed;
    init_label2(&fixed, 10, 20);
    return fixed.x == 10 && fixed.y == 20 &&
           strb_cptr(&fixed.sb) == fixed.buffer &&
           strb_len(&fixed.sb) == 0 && !strcmp(fixed.buffer, "");
}

int main(void)
{
    return test_label_initialisation() && test_editing() && fruit_sentence_example() && fruit_rows_example() &&
                   fruit_local_example() && test_short_write()
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
