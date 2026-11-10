// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strb.h"

#ifdef FORTIFY
#include "fortify.h"
#endif

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

// Reference output operations prepare space with strb_write, then fill it
// independently of the library's character, string and formatting wrappers.
static int ref_nputc(strb_t *const s, const int c, const size_t n)
{
    size_t i;
    _Optional char *const output = strb_write(s, n);
    if (!output)
        return EOF;
    for (i = 0; i < n; ++i)
        output[i] = (char)(unsigned char)c;
    return c;
}

static int ref_nputs(strb_t *const s, const char *const str, const size_t n)
{
    size_t i;
    size_t len = 0;
    while (len < n && str[len])
        ++len;
    {
        _Optional char *const output = strb_write(s, len);
        if (!output)
            return EOF;
        for (i = 0; i < len; ++i)
            output[i] = str[i];
        return 0;
    }
}

static int ref_puts(strb_t *const s, const char *const str)
{
    return ref_nputs(s, str, SIZE_MAX);
}

#if !STRB_FREESTANDING
static int ref_vputf(strb_t *const s, const char *const format, va_list args)
{
    // Formatting is independent of strb; use the returned count so embedded
    // null characters are written too. All test output fits in this array.
    char output[128];
    const int len = vsnprintf(output, sizeof output, format, args);
    assert(len >= 0);
    assert((size_t)len < sizeof output);
    {
        int i;
        _Optional char *const destination = strb_write(s, (size_t)len);
        if (!destination)
            return EOF;
        for (i = 0; i < len; ++i)
            destination[i] = output[i];
#if STRB_HAS_RESTORE
        // Exercise the direct writer's terminating null and boundary repair.
        destination[len] = '\0';
        strb_restore(s);
#endif
        return 0;
    }
}

static int ref_putf(strb_t *const s, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    {
        const int result = ref_vputf(s, format, args);
        va_end(args);
        return result;
    }
}

static int call_strb_vputf(strb_t *const s, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    {
        const int result = strb_vputf(s, format, args);
        va_end(args);
        return result;
    }
}
#endif

typedef struct {
    const char *name;
    const char *text;
    size_t count;
    int operation;
    int character;
} output_case;

enum { output_repeat, output_string, output_bounded,
       output_format, output_vformat };

// This source deliberately has no null terminator.
static const char bounded_output[] = {'X', 'Y', 'Z'};

static const output_case output_cases[] = {
    {"nputc zero", "", 0, output_repeat, 'X'},
    {"nputc one", "", 1, output_repeat, 'X'},
    {"nputc many", "", 12, output_repeat, 'X'},
    {"nputc null", "", 3, output_repeat, '\0'},
    {"nputc high byte", "", 3, output_repeat, 0x80},
    {"puts empty", "", 0, output_string, 0},
    {"puts one", "X", 0, output_string, 0},
    {"puts many", "0123456789AB", 0, output_string, 0},
    {"puts high byte", "x\200y", 0, output_string, 0},
    {"nputs zero", "XYZ", 0, output_bounded, 0},
    {"nputs empty", "", 4, output_bounded, 0},
    {"nputs bounded array", bounded_output, sizeof bounded_output, output_bounded, 0},
    {"nputs truncated", "XYZ", 2, output_bounded, 0},
    {"nputs early null", "X\0YZ", 4, output_bounded, 0},
    {"nputs many", "0123456789AB", 12, output_bounded, 0},
#if !STRB_FREESTANDING
    {"putf empty", "", 0, output_format, 0},
    {"putf conversions", "%s:%04d:%c:%%", 0, output_format, 'Q'},
    {"putf embedded null", "%s:%04d:%c:%%", 0, output_format, '\0'},
    {"vputf empty", "", 0, output_vformat, 0},
    {"vputf conversions", "%s:%04d:%c:%%", 0, output_vformat, 'Q'},
    {"vputf embedded null", "%s:%04d:%c:%%", 0, output_vformat, '\0'},
#endif
};

static int put_library_output(strb_t *const s, const output_case *const testcase)
{
    switch (testcase->operation) {
    case output_repeat:
        return strb_nputc(s, testcase->character, testcase->count);
    case output_string:
        return strb_puts(s, testcase->text);
    case output_bounded:
        return strb_nputs(s, testcase->text, testcase->count);
#if !STRB_FREESTANDING
    case output_format:
        return strb_putf(s, testcase->text, "ab", 7, testcase->character);
    case output_vformat:
        return call_strb_vputf(s, testcase->text, "ab", 7, testcase->character);
#endif
    default:
        assert(0);
        return EOF;
    }
}

static int put_reference_output(strb_t *const s, const output_case *const testcase)
{
    switch (testcase->operation) {
    case output_repeat:
        return ref_nputc(s, testcase->character, testcase->count);
    case output_string:
        return ref_puts(s, testcase->text);
    case output_bounded:
        return ref_nputs(s, testcase->text, testcase->count);
#if !STRB_FREESTANDING
    case output_format:
    case output_vformat:
        // ref_putf constructs the va_list passed to ref_vputf.
        return ref_putf(s, testcase->text, "ab", 7, testcase->character);
#endif
    default:
        assert(0);
        return EOF;
    }
}

static void compare_output(const strb_t *const actual, const strb_t *const reference,
                           const output_case *const testcase)
{
    const bool equal = strb_len(actual) == strb_len(reference) &&
                 strb_tell(actual) == strb_tell(reference) &&
                 strb_getmode(actual) == strb_getmode(reference) &&
                 strb_error(actual) == strb_error(reference) &&
                 !memcmp(strb_cptr(actual), strb_cptr(reference),
                         strb_len(reference) + 1);
    if (!equal) {
#ifdef __SDCC
        puts(testcase->name);
#else
        fprintf(stderr, "%s: actual len=%zu pos=%zu mode=%d error=%d; "
                        "reference len=%zu pos=%zu mode=%d error=%d\n",
                testcase->name, strb_len(actual), strb_tell(actual),
                strb_getmode(actual), strb_error(actual), strb_len(reference),
                strb_tell(reference), strb_getmode(reference),
                strb_error(reference));
#endif
    }
    assert(strb_len(actual) == strb_len(reference));
    assert(strb_tell(actual) == strb_tell(reference));
    assert(strb_getmode(actual) == strb_getmode(reference));
    assert(strb_error(actual) == strb_error(reference));
    assert(!memcmp(strb_cptr(actual), strb_cptr(reference),
                   strb_len(reference) + 1));
}

static void prepare_output(strb_t *const s, const char *const initial, const size_t pos,
                           const int mode, const bool pending_undo, const bool error)
{
    assert(!ref_puts(s, initial));
    assert(strb_setmode(s, mode) != EOF);
    strb_seek(s, pos);
    if (pending_undo)
        assert(strb_putc(s, 'q') == 'q');
    if (error) {
        assert(strb_setmode(s, -1) == EOF);
        assert(strb_error(s));
    }
}

static void check_output(strb_t *const actual, strb_t *const reference,
                         const output_case *const testcase)
{
    const int expected = put_reference_output(reference, testcase);
    assert(expected != EOF);
    assert(put_library_output(actual, testcase) == expected);
    compare_output(actual, reference, testcase);
#if STRB_HAS_RESTORE
    // Output functions promise that restore has no effect.
    {
        const size_t pos = strb_tell(actual);
        const char boundary = strb_cptr(actual)[pos];
        strb_restore(actual);
        assert(strb_cptr(actual)[pos] == boundary);
        strb_restore(reference);
        compare_output(actual, reference, testcase);
    }
#endif
#if STRB_HAS_UNPUTC
    // Only one undo is guaranteed, even after a multi-character write.
    assert(strb_unputc(actual) == strb_unputc(reference));
    compare_output(actual, reference, testcase);
#endif
}

static void test_output_equivalence(void)
{
    size_t t;
    enum { HAS_PENDING_UNDO = 1u << 0, HAS_ERROR = 1u << 1,
           HAS_PENDING_RESTORE = 1u << 2 };
    static const unsigned initial_states[] = {
        0, HAS_PENDING_UNDO, HAS_ERROR, HAS_PENDING_UNDO | HAS_ERROR,
#if STRB_HAS_RESTORE
        HAS_PENDING_RESTORE, HAS_PENDING_RESTORE | HAS_PENDING_UNDO,
        HAS_PENDING_RESTORE | HAS_ERROR,
        HAS_PENDING_RESTORE | HAS_PENDING_UNDO | HAS_ERROR,
#endif
    };
    static const char *const initial_strings[] = {"", "abcdef"};
    static const int modes[] = {strb_insert, strb_overwrite};
    for (t = 0; t < ARRAY_SIZE(output_cases); ++t) {
        size_t i;
        for (i = 0; i < ARRAY_SIZE(initial_strings); ++i) {
            size_t pos;
            const char *const initial = initial_strings[i];
            for (pos = 0; pos <= strlen(initial) + 2; ++pos) {
                size_t m;
                for (m = 0; m < ARRAY_SIZE(modes); ++m) {
                    size_t state_index;
                    for (state_index = 0;
                         state_index < ARRAY_SIZE(initial_states); ++state_index) {
                        const unsigned flags = initial_states[state_index];
                        static char actual_array[128], reference_array[128];
                        strb_t actual_state, reference_state;
                        strb_t *const actual = strb_use(&actual_state, sizeof actual_array,
                                                  actual_array);
                        strb_t *const reference = strb_use(&reference_state,
                                                     sizeof reference_array,
                                                     reference_array);
                        assert(actual);
                        assert(reference);
                        prepare_output(&*actual, initial, pos, modes[m],
                                       flags & HAS_PENDING_UNDO, flags & HAS_ERROR);
                        prepare_output(&*reference, initial, pos, modes[m],
                                       flags & HAS_PENDING_UNDO, flags & HAS_ERROR);
#if STRB_HAS_RESTORE
                        if (flags & HAS_PENDING_RESTORE) {
                            assert(!strb_split(&*actual));
                            assert(!strb_split(&*reference));
                        }
#endif
                        check_output(&*actual, &*reference, &output_cases[t]);
#if !STRB_FREESTANDING
                        strb_destroy(actual);
                        strb_destroy(reference);
#endif
#if !STRB_FREESTANDING
                        // Exercise owned storage as well as external arrays.
                        {
                            strb_t owned_actual = STRB_INITIALIZER(0),
                                   owned_reference = STRB_INITIALIZER(0);
                            prepare_output(&owned_actual, initial, pos, modes[m],
                                           flags & HAS_PENDING_UNDO, flags & HAS_ERROR);
                            prepare_output(&owned_reference, initial, pos, modes[m],
                                           flags & HAS_PENDING_UNDO, flags & HAS_ERROR);
#if STRB_HAS_RESTORE
                            if (flags & HAS_PENDING_RESTORE) {
                                assert(!strb_split(&owned_actual));
                                assert(!strb_split(&owned_reference));
                            }
#endif
                            check_output(&owned_actual, &owned_reference,
                                         &output_cases[t]);
                            strb_destroy(&owned_actual);
                            strb_destroy(&owned_reference);
                        }
#endif
                    }
                }
            }
        }
    }
}

#if STRB_HAS_UNPUTC
static void test_write_zero(void)
{
    char array[128];
    strb_t state;
    strb_t *const s = strb_use(&state, sizeof array, array);
    assert(s);
    prepare_output(&*s, "abc", 0, strb_overwrite, true, false);
    assert(strb_write(&*s, 0));
    assert(strb_tell(&*s) == 1);
    assert(strb_len(&*s) == 3);
    assert(strb_unputc(&*s) == 'q');
    assert(strb_tell(&*s) == 0);
    assert(!strcmp(strb_cptr(&*s), "abc"));
#if !STRB_FREESTANDING
    strb_destroy(s);
#endif
}
#endif

// Zero-count preparation exposes real storage, including its writable boundary.
static void test_write_zero_storage(void)
{
    static const int modes[] = {strb_insert, strb_overwrite};
    size_t mode;

    for (mode = 0; mode < ARRAY_SIZE(modes); ++mode) {
        char array[1];
        strb_t state;
        strb_t *const sb = strb_use(&state, sizeof array, array);
        _Optional char *output;

        strb_setmode(sb, modes[mode]);
        output = strb_write(sb, 0);
        assert(output == array);
        assert(output);
        *output = 'x';
#if STRB_HAS_RESTORE
        strb_restore(sb);
        assert(array[0] == '\0');
#else
        *output = '\0';
#endif
        assert(strb_len(sb) == 0);
        assert(strb_tell(sb) == 0);
        assert(!strb_error(sb));
#if !STRB_FREESTANDING
        strb_destroy(sb);
#endif
    }
}

// Unallocated storage cannot be exposed by any zero-length preparation.
static void test_empty_preparation(void)
{
    int operation;

    for (operation = 0; operation < 3; ++operation) {
        strb_t sb = STRB_INITIALIZER(0);
        char *const fallback = strb_ptr(&sb);
        int result;

        switch (operation) {
        case 0: result = strb_write(&sb, 0) ? 0 : EOF; break;
        case 1: result = strb_split(&sb); break;
        default: result = strb_cpy(&sb, ""); break;
        }
#if STRB_FREESTANDING
        assert(result == EOF);
        assert(strb_ptr(&sb) == fallback);
        assert(strb_error(&sb));
#else
        assert(result == 0);
        assert(strb_ptr(&sb) != fallback);
        assert(!strb_error(&sb));
        {
            _Optional char *const output = strb_write(&sb, 0);
            assert(output);
            *output = 'x';
#if STRB_HAS_RESTORE
            strb_restore(&sb);
            assert(*output == '\0');
#else
            *output = '\0';
#endif
        }
#endif
        assert(*fallback == '\0');
        assert(!strcmp(strb_cptr(&sb), ""));
        assert(strb_len(&sb) == 0);
        assert(strb_tell(&sb) == 0);
#if !STRB_FREESTANDING
        strb_destroy(&sb);
#endif
    }
}

#if !STRB_FREESTANDING
static void test_output_growth(void)
{
    size_t i, t;
    char initial[STRB_DFL_SIZE];
    static const int modes[] = {strb_insert, strb_overwrite};
    memset(initial, 'a', sizeof initial - 1);
    initial[sizeof initial - 1] = '\0';
    for (t = 0; t < ARRAY_SIZE(output_cases); ++t) {
        size_t m;
        for (m = 0; m < ARRAY_SIZE(modes); ++m) {
            strb_t actual = STRB_INITIALIZER(0), reference = STRB_INITIALIZER(0);
            // Nonempty output at the end must exceed the initial capacity.
            prepare_output(&actual, initial, strlen(initial), modes[m],
                           false, false);
            prepare_output(&reference, initial, strlen(initial), modes[m],
                           false, false);
            check_output(&actual, &reference, &output_cases[t]);
            strb_destroy(&actual);
            strb_destroy(&reference);
        }
    }
    // Seeking beyond allocated storage is allowed; the write obtains storage.
    {
        strb_t sb = STRB_INITIALIZER(0);
        const size_t pos = STRB_DFL_SIZE + sizeof "gap";
        strb_seek(&sb, pos);
        assert(strb_tell(&sb) == pos);
        assert(strb_len(&sb) == 0);
        assert(!strb_error(&sb));
        assert(strb_putc(&sb, 'X') == 'X');
        assert(strb_tell(&sb) == pos + 1);
        assert(strb_len(&sb) == pos + 1);
        for (i = 0; i < pos; ++i)
            assert(strb_cptr(&sb)[i] == '\0');
        assert(strb_cptr(&sb)[pos] == 'X');
        assert(strb_cptr(&sb)[pos + 1] == '\0');
        strb_destroy(&sb);
    }
}
#endif

// Exhausting a fixed external buffer must leave the operation's entire
// previous state intact, apart from the sticky error indicator. Compare with
// an untouched buffer so the oracle does not depend on strb_write failing.
static void test_output_failure(void)
{
    size_t t;
    enum { capacity = 8 };
    static const int modes[] = {strb_insert, strb_overwrite};
    for (t = 0; t < ARRAY_SIZE(output_cases); ++t) {
        size_t pos;
        const output_case *const testcase = &output_cases[t];
        bool too_large = testcase->count >= capacity ||
                         (testcase->operation == output_string &&
                          strlen(testcase->text) >= capacity);
#if !STRB_FREESTANDING
        too_large = too_large ||
                    ((testcase->operation == output_format ||
                      testcase->operation == output_vformat) &&
                     testcase->text[0]);
#endif
        if (!too_large)
            continue;
        for (pos = 0; pos <= 6; pos += 3) {
            size_t m;
            for (m = 0; m < ARRAY_SIZE(modes); ++m) {
                char actual_array[capacity], reference_array[capacity];
                strb_t actual_state, reference_state;
                strb_t *const actual = strb_use(&actual_state, sizeof actual_array,
                                          actual_array);
                strb_t *const reference = strb_use(&reference_state,
                                             sizeof reference_array,
                                             reference_array);
                assert(actual);
                assert(reference);
                prepare_output(&*actual, "abcdef", pos, modes[m], true, false);
                prepare_output(&*reference, "abcdef", pos, modes[m], true, false);
                assert(put_library_output(&*actual, testcase) == EOF);
                assert(strb_error(&*actual));
                strb_clearerr(&*actual);
                compare_output(&*actual, &*reference, testcase);
#if STRB_HAS_UNPUTC
                // Failure must preserve the previously available undo.
                assert(strb_unputc(&*actual) == strb_unputc(&*reference));
                compare_output(&*actual, &*reference, testcase);
#endif
#if !STRB_FREESTANDING
                strb_destroy(actual);
                strb_destroy(reference);
#endif
            }
        }
    }
}

static void test_delto_large_target(void)
{
    size_t t;
    static const size_t targets[] = {
        (size_t)STRB_MAX_SIZE - 1, STRB_MAX_SIZE,
        (size_t)STRB_MAX_SIZE + 1, (size_t)STRB_MAX_SIZE + 2, SIZE_MAX
    }, positions[] = {0, 3, 8};
    static const int modes[] = {strb_insert, strb_overwrite};
    for (t = 0; t < ARRAY_SIZE(targets); ++t) {
        size_t p;
        for (p = 0; p < ARRAY_SIZE(positions); ++p) {
            size_t m;
            for (m = 0; m < ARRAY_SIZE(modes); ++m) {
                char array[128];
                strb_t state;
                strb_t *const s = strb_use(&state, sizeof array, array);
                assert(s);
                assert(!strb_puts(&*s, "abcdef"));
                assert(strb_setmode(&*s, modes[m]) != EOF);
                strb_seek(&*s, positions[p]);
                strb_delto(&*s, targets[t]);
                {
                    size_t expected_len = strlen("abcdef");
                    if (modes[m] == strb_insert && positions[p] < expected_len)
                        expected_len = positions[p];
                    assert(strb_len(&*s) == expected_len);
                    assert(strb_tell(&*s) == positions[p]);
                    assert(!memcmp(strb_cptr(&*s), "abcdef", expected_len));
                    assert(strb_cptr(&*s)[expected_len] == '\0');
                    assert(!strb_error(&*s));
#if !STRB_FREESTANDING
                    strb_destroy(s);
#endif
                }
            }
        }
    }
}

// Setters return values that can restore the previous mode and position.
static void test_setter_restore(void)
{
    char array[16];
    strb_t state;
    strb_t *const sb = strb_use(&state, sizeof array, array);
    int old_mode, result;
    size_t old_pos, previous;

    old_mode = strb_setmode(sb, strb_overwrite);
    assert(old_mode == strb_insert);
    result = strb_setmode(sb, strb_overwrite);
    assert(result == strb_overwrite);
    result = strb_setmode(sb, old_mode);
    assert(result == strb_overwrite);
    assert(strb_getmode(sb) == strb_insert);

    result = strb_setmode(sb, -1);
    assert(result == EOF);
    assert(strb_getmode(sb) == strb_insert);
    assert(strb_error(sb));
    strb_clearerr(sb);

    old_pos = strb_seek(sb, 3);
    assert(old_pos == 0);
    previous = strb_seek(sb, old_pos);
    assert(previous == 3);
    assert(strb_tell(sb) == 0);
    previous = strb_seek(sb, SIZE_MAX);
    assert(previous == 0);
    old_pos = strb_seek(sb, 2);
    assert(old_pos == SIZE_MAX);
    previous = strb_seek(sb, old_pos);
    assert(previous == 2);
    assert(strb_tell(sb) == SIZE_MAX);
    assert(!strb_error(sb));
#if !STRB_FREESTANDING
    strb_destroy(sb);
#endif
}

static void test_seek(void)
{
    size_t i, m;
    static const char initial[] = "abcdef";
    static const size_t unsupported[] = {STRB_MAX_SIZE, (size_t)STRB_MAX_SIZE + 1, SIZE_MAX};
    static const int modes[] = {strb_insert, strb_overwrite};
    char array[64];
    strb_t state;
    strb_t *const s = strb_use(&state, sizeof array, array);
    assert(s);
    for (m = 0; m < ARRAY_SIZE(modes); ++m) {
        size_t pos, u;
        assert(!strb_cpy(&*s, initial));
        assert(strb_setmode(&*s, modes[m]) != EOF);
        for (pos = 0; pos <= strb_len(&*s); ++pos) {
            strb_seek(&*s, pos);
            assert(strb_tell(&*s) == pos);
            assert(strb_len(&*s) == sizeof initial - 1);
            assert(!memcmp(strb_cptr(&*s), initial, sizeof initial));
            assert(!strb_error(&*s));
        }

        // A supported seek beyond the end does not allocate or extend the buffer.
        {
            const size_t beyond_end = sizeof initial + 2;
            strb_seek(&*s, beyond_end);
            assert(strb_tell(&*s) == beyond_end);
            assert(strb_len(&*s) == sizeof initial - 1);
            assert(!strb_error(&*s));
            assert(strb_putc(&*s, 'X') == 'X');
            assert(strb_tell(&*s) == beyond_end + 1);
            assert(strb_len(&*s) == beyond_end + 1);
            assert(!memcmp(strb_cptr(&*s), initial, sizeof initial));
            for (pos = sizeof initial - 1; pos < beyond_end; ++pos)
                assert(strb_cptr(&*s)[pos] == '\0');
            assert(strb_cptr(&*s)[beyond_end] == 'X');
            assert(strb_cptr(&*s)[beyond_end + 1] == '\0');
        }

        for (u = 0; u < ARRAY_SIZE(unsupported); ++u) {
            size_t c;
            for (c = 0; c < ARRAY_SIZE(output_cases); ++c) {
                assert(!strb_cpy(&*s, initial));
                strb_seek(&*s, unsupported[u]);
                assert(strb_tell(&*s) == SIZE_MAX);
                assert(!strb_error(&*s));
                assert(put_library_output(&*s, &output_cases[c]) == EOF);
                assert(strb_error(&*s));
                assert(strb_tell(&*s) == SIZE_MAX);
                assert(strb_len(&*s) == sizeof initial - 1);
                assert(!memcmp(strb_cptr(&*s), initial, sizeof initial));
                strb_clearerr(&*s);
            }

            strb_seek(&*s, unsupported[u]);
#if STRB_HAS_UNPUTC
            assert(strb_unputc(&*s) == EOF);
            assert(strb_error(&*s));
            strb_clearerr(&*s);
#endif
            assert(strb_putc(&*s, 'X') == EOF);
            assert(strb_error(&*s));
            strb_clearerr(&*s);
            assert(!strb_write(&*s, 0));
            assert(strb_error(&*s));
            strb_clearerr(&*s);
            assert(!strb_write(&*s, 1));
            assert(strb_error(&*s));
            strb_clearerr(&*s);
            assert(strb_split(&*s) == EOF);
            assert(strb_error(&*s));
            assert(strb_tell(&*s) == SIZE_MAX);
            assert(strb_len(&*s) == sizeof initial - 1);
            assert(!memcmp(strb_cptr(&*s), initial, sizeof initial));
#if STRB_HAS_RESTORE
            strb_restore(&*s);
            assert(!memcmp(strb_cptr(&*s), initial, sizeof initial));
#endif
            // A successful seek recovers the position but preserves the error.
            strb_seek(&*s, 0);
            assert(strb_tell(&*s) == 0);
            assert(strb_error(&*s));
            strb_clearerr(&*s);
            assert(strb_putc(&*s, 'X') == 'X');
            assert(strb_cptr(&*s)[0] == 'X');
            assert(!strb_error(&*s));
            assert(!strb_cpy(&*s, initial));

            // Deletion treats an unsupported current position as SIZE_MAX.
            strb_seek(&*s, unsupported[u]);
            strb_delto(&*s, unsupported[u]);
            assert(strb_tell(&*s) == SIZE_MAX);
            assert(!memcmp(strb_cptr(&*s), initial, sizeof initial));
            strb_delto(&*s, sizeof initial - 1);
            assert(strb_tell(&*s) == sizeof initial - 1);
            assert(!memcmp(strb_cptr(&*s), initial, sizeof initial));
            strb_seek(&*s, unsupported[u]);
            strb_delto(&*s, 0);
            assert(strb_tell(&*s) == 0);
            assert(strb_len(&*s) == (modes[m] == strb_insert ? 0 : sizeof initial - 1));
            assert(!strb_error(&*s));
            assert(!strb_cpy(&*s, initial));

            // Whole-string replacement also recovers from an unsupported position.
            strb_seek(&*s, unsupported[u]);
            assert(!strb_cpy(&*s, initial));
            assert(strb_tell(&*s) == sizeof initial - 1);
        }
    }
    // Splitting beyond the length succeeds when the external array has room.
    {
        const size_t split_pos = strb_len(&*s) + sizeof "gap";
        strb_seek(&*s, split_pos);
        assert(!strb_split(&*s));
        assert(strb_tell(&*s) == split_pos);
        assert(strb_len(&*s) == split_pos);
        assert(!strb_error(&*s));
        for (i = sizeof initial - 1; i <= split_pos; ++i)
            assert(strb_cptr(&*s)[i] == '\0');
        assert(!strb_cpy(&*s, initial));
        // Even a representable position can require unavailable storage at output time.
        strb_seek(&*s, STRB_MAX_SIZE - 1);
        assert(strb_tell(&*s) == STRB_MAX_SIZE - 1);
        assert(!strb_error(&*s));
        assert(strb_split(&*s) == EOF);
        assert(strb_error(&*s));
        assert(!memcmp(strb_cptr(&*s), initial, sizeof initial));
#if !STRB_FREESTANDING
        strb_destroy(s);
#endif
    }
}

// Emptying preserves storage, mode and errors while discarding saved characters.
static void test_empty(strb_t *const sb)
{
    char *storage;
    assert(!strb_cpy(sb, "abc"));
    storage = strb_ptr(sb);
    assert(strb_setmode(sb, strb_overwrite) != EOF);
    assert(strb_setmode(sb, -1) == EOF);

    strb_empty(sb);
    assert(strb_len(sb) == 0);
    assert(strb_tell(sb) == 0);
    assert(!strcmp(strb_cptr(sb), ""));
    assert(strb_ptr(sb) == storage);
    assert(strb_getmode(sb) == strb_overwrite);
    assert(strb_error(sb));
    strb_clearerr(sb);

#if STRB_HAS_RESTORE
    assert(!strb_puts(sb, "abc"));
    strb_seek(sb, 1);
    assert(!strb_split(sb));
    strb_empty(sb);
    strb_restore(sb);
    assert(!strcmp(strb_cptr(sb), ""));
#endif
#if STRB_HAS_UNPUTC
    assert(strb_putc(sb, 'a') == 'a');
    strb_empty(sb);
    assert(strb_unputc(sb) == EOF);
    strb_clearerr(sb);
#endif

    // Repeat on an already empty buffer, then reuse its retained storage.
    strb_empty(sb);
    assert(!strb_error(sb));
    assert(!strb_puts(sb, "abc"));
    assert(strb_ptr(sb) == storage);
    assert(!strcmp(strb_cptr(sb), "abc"));
    strb_empty(sb);
    assert(strb_setmode(sb, strb_insert) != EOF);
}

static void test(strb_t *const s)
{
    char *c;
    int i;
    const char *found;
    size_t pos;

    for (i = 5; i >= 0; --i) {
        strb_seek(s, 0);
        assert(strb_putc(s, 'a' + i) == 'a' + i);
        assert(strb_ptr(s)[strb_len(s)] == '\0');
#if !STRB_FREESTANDING
        assert(!strb_putf(s, "fmt%dx", i));
        assert(strb_ptr(s)[strb_len(s)] == '\0');
#if STRB_HAS_UNPUTC
        assert(strb_unputc(s) == 'x');
#endif
#else
#if STRB_HAS_UNPUTC
        assert(strb_unputc(s) == 'a' + i);
#endif
#endif // !STRB_FREESTANDING
        assert(strb_ptr(s)[strb_len(s)] == '\0');
#if STRB_HAS_UNPUTC
        assert(strb_unputc(s) == EOF);
#endif
        assert(strb_ptr(s)[strb_len(s)] == '\0');
        assert(!strb_puts(s, "str"));
        assert(strb_ptr(s)[strb_len(s)] == '\0');
    }

    puts(strb_ptr(s));

    assert(strb_setmode(s, strb_overwrite) != EOF);
    assert(strb_getmode(s) == strb_overwrite);

    assert(!strb_puts(s, "OVERWRITE"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));

    strb_seek(s, strb_len(s) - 2);
    assert(!strb_puts(s, "OVERWRITE"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));

    assert(strb_setmode(s, strb_insert) != EOF);
    assert(strb_getmode(s) == strb_insert);

    found = strstr(strb_ptr(s), "fmt4");
    if (found) {
        strb_seek(s, (size_t)(found - strb_ptr(s)));
        printf("%zu\n", strb_tell(s));
        assert(!strb_puts(s, "INSERT"));
        assert(strb_ptr(s)[strb_len(s)] == '\0');
        puts(strb_ptr(s));
    }

    strb_seek(s, strb_len(s) + 2);
    pos = strb_tell(s);
    assert(!strb_puts(s, "BEYOND"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    puts(strb_ptr(s) + pos);
    assert(!strcmp(strb_ptr(s) + pos, "BEYOND"));

    strb_delto(s, 0);
    assert(strb_tell(s) == 0);
    assert(strb_len(s) == 0);
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));

    assert(!strb_puts(s, "DELETEME"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    assert(!strcmp(strb_ptr(s), "DELETEME"));
    assert(strb_len(s) == strlen("DELETEME"));

    strb_delto(s, strb_tell(s)); // no-op
    assert(strb_tell(s) == strlen("DELETEME"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "DELETEME"));
    assert(strb_len(s) == 8);

    {
        const size_t lo = strlen("DELETEME") + 1, hi = lo + 1;

        strb_seek(s, lo);
        assert(strb_tell(s) == lo);
        strb_delto(s, hi); // no-op
        assert(strb_tell(s) == lo);

        strb_seek(s, hi);
        assert(strb_tell(s) == hi);
        strb_delto(s, lo); // reposition only
        assert(strb_tell(s) == lo);
        assert(strb_ptr(s)[strb_len(s)] == '\0');
        puts(strb_ptr(s));
        assert(!strcmp(strb_ptr(s), "DELETEME"));
        assert(strb_len(s) == strlen("DELETEME"));

        strb_delto(s, SIZE_MAX); // no-op
        assert(strb_tell(s) == lo);
        assert(strb_ptr(s)[strb_len(s)] == '\0');
        puts(strb_ptr(s));
        assert(!strcmp(strb_ptr(s), "DELETEME"));
        assert(strb_len(s) == strlen("DELETEME"));
    }

    strb_seek(s, strlen("DELETEM"));
    strb_delto(s, SIZE_MAX); // delete "E"
    assert(strb_tell(s) == strlen("DELETEM"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "DELETEM"));
    assert(strb_len(s) == strlen("DELETEM"));

    strb_delto(s, strlen("DEL")); // delete "ETEM"
    assert(strb_tell(s) == strlen("DEL"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "DEL"));
    assert(strb_len(s) == strlen("DEL"));

    strb_seek(s, strlen("D"));
    strb_delto(s, strlen("DE")); // delete "E"
    assert(strb_tell(s) == strlen("D"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "DL"));
    assert(strb_len(s) == strlen("DL"));

    strb_delto(s, 0); // delete "D"
    assert(strb_tell(s) == 0);
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "L"));
    assert(strb_len(s) == strlen("L"));

    assert(!strb_puts(s, "FEE")); // make "FEEL"
    assert(strb_tell(s) == strlen("FEE"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "FEEL"));
    assert(strb_len(s) == strlen("FEEL"));

    assert(!strb_cpy(s, "No"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    assert(!strcmp(strb_ptr(s), "No"));
    assert(strb_len(s) == 2);
    puts(strb_ptr(s));

    assert(!strb_ncpy(s, "Nope", 5));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    assert(!strcmp(strb_ptr(s), "Nope"));
    assert(strb_len(s) == 4);
    puts(strb_ptr(s));

    assert(!strb_ncpy(s, "Nope", 3));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    assert(!strcmp(strb_ptr(s), "Nop"));
    assert(strb_len(s) == 3);
    puts(strb_ptr(s));

#if !STRB_FREESTANDING
    assert(!strb_printf(s, "R%dD%d", 2, 2));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    assert(!strcmp(strb_ptr(s), "R2D2"));
    assert(strb_len(s) == 4);
    assert(strb_tell(s) == 4);
    puts(strb_ptr(s));

    strb_seek(s, 2);
    {
        _Optional char *const w = strb_write(s, 0);
        assert(w);
        *w = '\0';
    }

    assert(!strcmp(strb_ptr(s), "R2"));
    assert(strb_len(s) == 4);
    assert(strb_tell(s) == 2);
    puts(strb_ptr(s));

#if STRB_HAS_RESTORE
    strb_restore(s);
    assert(!strcmp(strb_ptr(s), "R2D2"));
    assert(strb_len(s) == 4);
    assert(strb_tell(s) == 2);
    puts(strb_ptr(s));
#endif

    strb_seek(s, strb_len(s));
    {
        _Optional char *const w = strb_write(s, 0);
        assert(w);
        *w = 'q'; // probably illegal!
    }
    assert(strb_ptr(s)[strb_len(s)] == 'q');

#if STRB_HAS_RESTORE
    strb_restore(s);
    assert(strb_ptr(s)[strb_len(s)] == '\0');
#endif

    assert(!strb_printf(s, "C%dP%d", 3, 0));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    assert(!strcmp(strb_ptr(s), "C3P0"));
    assert(strb_len(s) == 4);
    assert(strb_tell(s) == 4);
    puts(strb_ptr(s));

    strb_seek(s, 2);
    assert(!strb_split(s));
    assert(!strcmp(strb_ptr(s), "C3"));
    assert(strb_len(s) == 4);
    assert(strb_tell(s) == 2);
    puts(strb_ptr(s));

#if STRB_HAS_RESTORE
    strb_restore(s);
    assert(!strcmp(strb_ptr(s), "C3P0"));
    assert(strb_len(s) == 4);
    assert(strb_tell(s) == 2);
    puts(strb_ptr(s));
#endif

    strb_seek(s, strb_len(s));
    assert(!strb_split(s));
    assert(strb_ptr(s)[strb_len(s)] == '\0');

#if STRB_HAS_RESTORE
    strb_restore(s);
    assert(strb_ptr(s)[strb_len(s)] == '\0');
#endif

#endif // !STRB_FREESTANDING

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    {
        strb_t const *const sc = s;
        const char *const q = strb_ptr(sc);
        assert(strb_ptr(sc)[strb_len(sc)] == '\0');
        puts(q);
    }
#endif

    {
        strb_t const *const sc = s;
        const char *const q = strb_cptr(sc);
        assert(strb_cptr(sc)[strb_len(sc)] == '\0');
        puts(q);
    }

    for (c = strb_ptr(s); *c != '\0'; ++c) {
        *c = (char)tolower((unsigned char)(*c));
    }
    puts(strb_cptr(s));

    for (c = strb_ptr(s); *c != '\0'; ++c) {
        *c = (char)toupper((unsigned char)(*c));
    }
    puts(strb_cptr(s));

    puts("========");
}

static void test_reuse_contents(void)
{
    static const struct {
        size_t size, terminator;
    } cases[] = {
        {1, 1},   // Reject unterminated input in a one-character extent.
        {1, 0},   // Preserve a one-character empty string.
        {64, 0},  // Preserve an empty string and its unused suffix.
        {64, 7},  // Preserve a string ending before the extent's boundary.
        {64, 63}, // Preserve a string ending at the extent's boundary.
        {64, 64}  // Reject an extent containing no null character.
    };
    size_t i;

    for (i = 0; i < ARRAY_SIZE(cases); ++i) {
        struct {
            char before, buf[64], after;
        } array;
        strb_t *sb;
        const size_t size = cases[i].size, terminator = cases[i].terminator;
        const size_t len = terminator < size ? terminator : 0;
        size_t j;
        strb_t state;

        array.before = 'b';
        array.after = 'c';
        memset(array.buf, 'a', sizeof array.buf);
        if (terminator < size) {
            array.buf[terminator] = '\0';
        }

        // Preserve supported strings, or construct an empty buffer with an error.
        sb = strb_reuse(&state, size, array.buf);
        assert(strb_len(sb) == len);
        assert(strb_tell(sb) == len);
        assert(strb_getmode(sb) == strb_insert);
        assert(strb_error(sb) == (terminator >= size));
        assert(strlen(strb_cptr(sb)) == len);

        // Neither the unused suffix nor characters outside the extent change.
        for (j = 0; j < sizeof array.buf; ++j) {
            assert(array.buf[j] == (j == len ? '\0' : 'a'));
        }

        assert(array.before == 'b');
        assert(array.after == 'c');
#if STRB_HAS_UNPUTC
        assert(strb_unputc(sb) == (len ? 'a' : EOF));
#endif
        if (terminator >= size) {
            // Successful output does not clear the construction error.
            assert(!strb_puts(sb, size > 1 ? "b" : ""));
            assert(strb_error(sb));
            strb_clearerr(sb);
            assert(!strb_error(sb));
        }

#if !STRB_FREESTANDING
        strb_destroy(sb);
#endif
    }
}

// Fixed character storage must not fall back to library allocation.
static void test_decl_macro(void)
{
    STRB_DECL_FIXED(first, 3);
    STRB_DECL_FIXED(second, 3);
    STRB_DECL_FIXED(empty, 0);

    assert(strb_len(first) == 0);
    assert(!strb_puts(first, "abc"));
    assert(strb_putc(first, 'd') == EOF);
    assert(!strcmp(strb_cptr(first), "abc"));
    assert(strb_len(second) == 0);
    assert(!strb_puts(second, "xy"));
    assert(!strcmp(strb_cptr(first), "abc"));
    assert(!strcmp(strb_cptr(second), "xy"));
    assert(strb_putc(empty, 'a') == EOF);
    assert(!strcmp(strb_cptr(empty), ""));
}

// Explicit state wrappers derive the whole array extent and evaluate state once.
static void test_array_state_macros(void)
{
    char empty[16] = "discard", existing[16] = "preserve";
    strb_t states[2], *next = states;
    strb_t *const first = STRB_USE(next++, empty),
           *const second = STRB_REUSE(next++, existing);

    assert(next == states + 2);
    assert(empty[0] == '\0');
    assert(!strb_puts(first, "123456789012345"));
    assert(strb_putc(first, '6') == EOF);
    assert(!strcmp(empty, "123456789012345"));
    assert(!strb_puts(second, " text"));
    assert(!strcmp(existing, "preserve text"));
}

#ifndef __CC65__
static void test_array_macros(void)
{
    // SDCC Z80 r17140 miscompiles *p++ when p points to an array.
    char empty[1][64] = {"discard"}, existing[1][64] = {"preserve"};
    size_t next = 0, reuse_next = 0;
#ifdef __SDCC
    // SDCC r17140 reinitialises the temporary wrappers' compound-literal
    // state after the call. Use named state until that compiler bug is fixed.
    strb_t first_state, second_state;
    strb_t *const first = STRB_USE(&first_state, empty[next++]),
           *const second = STRB_REUSE(&second_state, existing[reuse_next++]);
#else
    strb_t *const first = STRB_TMP_USE(empty[next++]),
           *const second = STRB_TMP_REUSE(existing[reuse_next++]);
#endif

    assert(next == 1);
    assert(reuse_next == 1);
    assert(empty[0][0] == '\0');
    assert(!strcmp(strb_cptr(second), "preserve"));
#ifdef __SDCC
    {
        STRB_DECL_FIXED(single, 0);
        assert(strb_putc(single, 'a') == EOF);
    }
#else
    assert(strb_putc(STRB_TMP_USE((char[1]){0}), 'a') == EOF);
#endif
    assert(!strb_puts(first, "new"));
    assert(!strb_puts(second, " text"));
    assert(!strcmp(empty[0], "new"));
    assert(!strcmp(existing[0], "preserve text"));
#if !STRB_FREESTANDING
    strb_destroy(first);
    strb_destroy(second);
#endif
}
#endif

// Zero-initialisation supplies usable empty metadata in every configuration.
static void test_initial_value(void)
{
    static strb_t empty;
    strb_t local = STRB_INITIALIZER(0), copy;
    const strb_t *const readonly = &empty;

    copy = local;
    assert(!strcmp(strb_cptr(readonly), ""));
    assert(!strb_len(&local));
    assert(!strb_tell(&local));
    assert(strb_getmode(&local) == strb_insert);
    assert(!strb_error(&local));
    assert(strb_ptr(&local) != strb_ptr(&copy));
#if STRB_FREESTANDING
    assert(!strb_write(&local, 0));
    assert(strb_error(&local));
#else
    assert(strb_write(&local, 0));
#if STRB_HAS_RESTORE
    strb_restore(&local);
#endif
#endif
#if !STRB_FREESTANDING
    strb_destroy(&local);
#endif
    assert(!strcmp(strb_cptr(&copy), ""));
    assert(!strb_error(&copy));
}

#if !STRB_FREESTANDING
static void test_alloc_macro(void)
{
    size_t hint = 0;
    STRB_DECL_ALLOC(first, hint++);
    STRB_DECL_ALLOC(second, 0);
    unsigned i;

    assert(hint == 1);
    assert(!strb_puts(first, "first"));
    assert(!strb_puts(second, "second"));
    assert(!strcmp(strb_cptr(first), "first"));
    assert(!strcmp(strb_cptr(second), "second"));
    strb_destroy(first);
    strb_destroy(second);

    for (i = 0; i < 3; ++i) {
        STRB_DECL_ALLOC(sb, 0);
        // Exercise the handle after its declaration's full expression ends.
        test(sb);
        strb_destroy(sb);
    }
}

// Initial values are independently usable; live relocation transfers ownership.
static void test_object_lifetime(void)
{
    static strb_t static_empty;
    strb_t first = STRB_INITIALIZER(0), second = first, moved;
    struct {
        int x, y;
        strb_t text;
    } label = {0};
    size_t i;

    assert(!strcmp(strb_cptr(&static_empty), ""));
    assert(!strcmp(strb_cptr(&label.text), ""));
    assert(strb_ptr(&first) != strb_ptr(&second));
    assert(strb_write(&first, 0));
    assert(!strb_error(&first));
    strb_destroy(&first);
    strb_init(&first, 0);
    assert(!strb_puts(&first, "first"));
    assert(!strb_puts(&second, "second"));
    assert(!strcmp(strb_cptr(&first), "first"));
    assert(!strcmp(strb_cptr(&second), "second"));

    // Abandon first after transfer; do not destroy both representations.
    moved = first;
    strb_init(&first, 0);
    assert(!strcmp(strb_cptr(&moved), "first"));
    assert(strb_nputc(&moved, 'x', STRB_DFL_SIZE * 3u) == 'x');
    assert(strb_len(&moved) == 5 + STRB_DFL_SIZE * 3u);
    for (i = 5; i < strb_len(&moved); ++i) {
        assert(strb_cptr(&moved)[i] == 'x');
    }
    strb_destroy(&moved);
    strb_destroy(&second);

    strb_seek(&first, 3);
    assert(strb_write(&first, 0));
    assert(strb_len(&first) == 3);
    assert(!memcmp(strb_cptr(&first), "\0\0\0", 4));
    strb_destroy(&first);
    test(&label.text);
    strb_destroy(&label.text);
}
#endif

#if !STRB_FREESTANDING
static int call_vprintf(strb_t *const sb, const char *const format, ...)
{
    int result;
    va_list args;
    va_start(args, format);
    result = strb_vprintf(sb, format, args);
    va_end(args);
    return result;
}
#endif

// Failed replacement must preserve positioned text and pending undo state.
static void test_replacement_failure(void)
{
    static const int modes[] = {strb_insert, strb_overwrite};
    size_t mode;
    int operation;
    for (mode = 0; mode < ARRAY_SIZE(modes); ++mode) {
        for (operation = 0; operation < 4; ++operation) {
            char array[8] = {0}, saved[8];
            strb_t state;
            strb_t *const sb = strb_use(&state, sizeof array, array);
            _Optional char *out;
            size_t length;
            int result;
            strb_cpy(sb, "abcdef");
            strb_setmode(sb, modes[mode]);
            strb_seek(sb, 2);
            out = strb_write(sb, 1);
            assert(out);
            *out = 'X';
#if STRB_HAS_RESTORE
            strb_split(sb);
#endif
            length = strb_len(sb);
            memcpy(saved, array, sizeof saved);
            switch (operation) {
            case 0: result = strb_cpy(sb, "too long"); break;
            case 1: result = strb_ncpy(sb, "too longTAIL", 8); break;
#if !STRB_FREESTANDING
            case 2: result = strb_printf(sb, "%s", "too long"); break;
            default: result = call_vprintf(sb, "%s", "too long"); break;
#else
            default: continue;
#endif
            }
            assert(result == EOF);
            assert(strb_error(sb));
            assert(strb_tell(sb) == 3);
            assert(strb_len(sb) == length);
            assert(strb_getmode(sb) == modes[mode]);
            assert(!memcmp(saved, array, sizeof saved));
#if STRB_HAS_RESTORE
            strb_restore(sb);
#endif
#if STRB_HAS_UNPUTC
            result = strb_unputc(sb);
            assert(result == 'X');
            assert(!strcmp(strb_cptr(sb), "abcdef"));
#endif
#if !STRB_FREESTANDING
            strb_destroy(sb);
#endif
        }
    }
}

#if defined(FORTIFY) && !STRB_FREESTANDING
// A failed zero-length preparation preserves the fallback and permits retry.
static void test_empty_preparation_failure(void)
{
    int operation;

    for (operation = 0; operation < 3; ++operation) {
        strb_t sb = STRB_INITIALIZER(0);
        char *const fallback = strb_ptr(&sb);
        int result;

        Fortify_SetNumAllocationsLimit(0);
        switch (operation) {
        case 0: result = strb_write(&sb, 0) ? 0 : EOF; break;
        case 1: result = strb_split(&sb); break;
        default: result = strb_cpy(&sb, ""); break;
        }
        Fortify_SetNumAllocationsLimit(ULONG_MAX);
        assert(result == EOF);
        assert(strb_error(&sb));
        assert(strb_ptr(&sb) == fallback);
        assert(*fallback == '\0');
        assert(strb_len(&sb) == 0);
        assert(strb_tell(&sb) == 0);
        strb_clearerr(&sb);
        assert(strb_write(&sb, 0));
        Fortify_SetNumAllocationsLimit(0);
        assert(strb_write(&sb, 0));
        Fortify_SetNumAllocationsLimit(ULONG_MAX);
        assert(!strb_error(&sb));
        strb_destroy(&sb);
    }
}

static int replace_for_test(strb_t *const sb, const int operation, const char *const text)
{
    switch (operation) {
    case 0: return strb_cpy(sb, text);
    case 1: return strb_ncpy(sb, text, strlen(text));
    default: return strb_printf(sb, "%s", text);
    }
}

// Failed constructors and cleanup must release every library-owned allocation.
static void test_constructor_cleanup(void)
{
    const unsigned long before = Fortify_GetCurrentAllocation();
    unsigned long limit;
    int constructor;

    void (*const old_destroy)(_Optional strb_t *) = &strb_free;
    old_destroy(NULL);
    for (constructor = 0; constructor < 4; ++constructor) {
        for (limit = 0; limit < 3; ++limit) {
            _Optional strb_t *sb;
            Fortify_SetNumAllocationsLimit(limit);
            switch (constructor) {
            case 0: sb = strb_alloc(0); break;
            case 1: sb = strb_dup("text"); break;
            case 2: sb = strb_ndup("text tail", 4); break;
            default: sb = strb_aprintf("%s", "text"); break;
            }
            Fortify_SetNumAllocationsLimit(ULONG_MAX);
            assert((sb != NULL) == (limit > (constructor ? 1u : 0u)));
            if (sb && constructor) {
                assert(!strcmp(strb_cptr(&*sb), "text"));
            }
            strb_destroy(sb);
            assert(Fortify_GetCurrentAllocation() == before);
        }
    }
}

// Both initialisation forms use their hint for the first character allocation.
static void test_allocation_hint(void)
{
    int constructor;
    for (constructor = 0; constructor < 2; ++constructor) {
        strb_t sb = STRB_INITIALIZER(1024);
        if (constructor) {
            strb_init(&sb, 1024);
        }
        assert(strb_putc(&sb, 'x') == 'x');
        Fortify_SetNumAllocationsLimit(0);
        assert(strb_nputc(&sb, 'x', 1023) == 'x');
        Fortify_SetNumAllocationsLimit(ULONG_MAX);
        assert(strb_len(&sb) == 1024);
        strb_destroy(&sb);
    }
}

// Allocation failure preserves the previous string and editing state.
static void test_replacement_allocation_failure(void)
{
    char longer[300];
    int operation;
    memset(longer, 'A', sizeof longer - 1);
    longer[sizeof longer - 1] = '\0';
    for (operation = 0; operation < 4; ++operation) {
        STRB_DECL_ALLOC(sb, 0);
        int result;
        strb_setmode(sb, strb_overwrite);
        strb_seek(sb, 4);
        Fortify_SetNumAllocationsLimit(0);
        result = operation == 3 ? call_vprintf(sb, "%s", "new")
                                : replace_for_test(sb, operation, "new");
        Fortify_SetNumAllocationsLimit(ULONG_MAX);
        assert(result == EOF);
        assert(!strcmp(strb_cptr(sb), ""));
        assert(strb_len(sb) == 0);
        assert(strb_tell(sb) == 4);
        assert(strb_getmode(sb) == strb_overwrite);
        assert(strb_error(sb));
        strb_clearerr(sb);
        result = strb_cpy(sb, "old");
        assert(result == 0);
        strb_seek(sb, 1);
        strb_split(sb);
        Fortify_SetNumAllocationsLimit(0);
        result = operation == 3 ? call_vprintf(sb, "%s", longer)
                                : replace_for_test(sb, operation, longer);
        Fortify_SetNumAllocationsLimit(ULONG_MAX);
        assert(result == EOF);
        assert(!strcmp(strb_cptr(sb), "o"));
        assert(strb_len(sb) == 3);
        assert(strb_tell(sb) == 1);
        assert(strb_getmode(sb) == strb_overwrite);
        assert(strb_error(sb));
#if STRB_HAS_RESTORE
        strb_restore(sb);
        assert(!strcmp(strb_cptr(sb), "old"));
#endif
        strb_destroy(sb);
    }
}
#endif

int main(void)
{
    static char array[1000];
#if STRB_FREESTANDING
    strb_t *s;
#else
    _Optional strb_t *s;
#endif
    int c;

    strb_t state;

#if !STRB_FREESTANDING
    test_object_lifetime();
    test_alloc_macro();
#endif
    test_initial_value();
    test_reuse_contents();
    test_decl_macro();
    test_array_state_macros();
#ifndef __CC65__
    test_array_macros();
#endif
    test_output_equivalence();
    test_write_zero_storage();
    test_empty_preparation();
#if STRB_HAS_UNPUTC
    test_write_zero();
#endif
#if !STRB_FREESTANDING
    test_output_growth();
#endif
    test_output_failure();
    test_replacement_failure();
#if defined(FORTIFY) && !STRB_FREESTANDING
    test_replacement_allocation_failure();
    test_empty_preparation_failure();
    test_allocation_hint();
    test_constructor_cleanup();
#endif
    test_delto_large_target();
    test_setter_restore();
    test_seek();


    s = strb_use(&state, sizeof array, array);
    if (!s) {
        assert(s);
        return 1;
    }
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    test_empty(&*s);
    test(&*s);
    // Reuse stops at the first null, including an unrestored split.
    c = array[strlen(array) - 1];

    s = strb_reuse(&state, sizeof array, array);
    if (!s) {
        assert(s);
        return 1;
    }
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == c);
#else
    (void)c;
#endif

    test(&*s);

    memset(array, 'a', sizeof array);
    // No null terminator.
    s = strb_reuse(&state, sizeof array, array);
    assert(s);
    assert(strb_len(&*s) == 0);
    assert(strb_tell(&*s) == 0);
    assert(strb_error(&*s));
    assert(array[0] == '\0');
    assert(array[1] == 'a');

    strcpy(array,
             "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
             "Curabitur lacinia mi "
             "mollis, tincidunt ipsum ut, commodo massa. Maecenas sit amet "
             "mattis augue. Fusce "
             "bibendum condimentum tortor accumsan sodales. Curabitur "
             "accumsan, ante sit amet "
             "commodo massa nunc. ");
    {
        const size_t len = strlen(array);
        s = strb_reuse(&state, sizeof array, array);
        assert(s);
        if (len >= STRB_MAX_SIZE) {
            // A valid string beyond the implementation limit is not adopted.
            assert(strb_len(&*s) == 0);
            assert(strb_tell(&*s) == 0);
            assert(strb_error(&*s));
            assert(array[0] == '\0');
            assert(array[1] == 'o');
        } else {
            assert(strb_len(&*s) == len);
            assert(strb_tell(&*s) == len);
            assert(!strb_error(&*s));
        }

        puts(strb_ptr(&*s));
    }

#if !STRB_FREESTANDING
    s = strb_alloc(2700);
    assert(s);
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    test_empty(&*s);
    test(&*s);
    strb_destroy(s);

    s = strb_alloc(5);
    assert(s);
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    test(&*s);
    strb_destroy(s);

    s = strb_alloc(5000);
    assert(s);
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    strb_write(&*s, 0);
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    test(&*s);
    strb_destroy(s);

    s = strb_ndup("", 0);
    assert(s);
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif
    strb_destroy(s);

    s = strb_dup("");
    assert(s);
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    assert(!strb_cpy(&*s, ""));
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    assert(!strb_ncpy(&*s, "", 0));
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    strb_destroy(s);

    s = strb_aprintf("");
    assert(s);
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif
    assert(!strb_printf(&*s, ""));
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    strb_destroy(s);

    s = strb_dup("DUPLICATE");
    assert(s);
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == 'E');
    assert(strb_unputc(&*s) == EOF);
#endif
    test(&*s);
    strb_destroy(s);

    s = strb_ndup("DUPLICATE", 3);
    assert(s);
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == 'P');
    assert(strb_unputc(&*s) == EOF);
#endif

    test(&*s);
    strb_destroy(s);

    s = strb_aprintf("Hello %d", 99);
    assert(s);
#if STRB_HAS_UNPUTC
    assert(strb_unputc(&*s) == '9');
    assert(strb_unputc(&*s) == EOF);
#endif

    test(&*s);
    strb_destroy(s);

    s = strb_dup("Lorem ipsum dolor sit amet");
    assert(s);
    puts(strb_ptr(&*s));
    strb_destroy(s);

    s = strb_dup("Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                 "Curabitur lacinia mi "
                 "mollis, tincidunt ipsum ut, commodo massa. Maecenas sit amet "
                 "mattis augue. Fusce "
                 "bibendum condimentum tortor accumsan sodales. Curabitur "
                 "accumsan, ante sit amet "
                 "commodo massa nunc. ");
#if STRB_MAX_SIZE <= UINT8_MAX
    assert(!s); // too long
#else
    assert(s);
    puts(strb_ptr(&*s));
#endif
    strb_destroy(s);

#endif // !STRB_FREESTANDING
    return 0;
}
