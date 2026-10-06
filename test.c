// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#undef NDEBUG
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strb.h"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#if STRB_EXT_STATE || !STRB_FREESTANDING
// Reference output operations prepare space with strb_write, then fill it
// independently of the library's character, string and formatting wrappers.
static int ref_nputc(strb_t *s, int c, size_t n)
{
    size_t i;
    _Optional char *output = strb_write(s, n);
    if (!output)
        return EOF;
    for (i = 0; i < n; ++i)
        output[i] = (char)(unsigned char)c;
    return c;
}

static int ref_nputs(strb_t *s, const char *str, size_t n)
{
    size_t i;
    size_t len = 0;
    while (len < n && str[len])
        ++len;
    {
        _Optional char *output = strb_write(s, len);
        if (!output)
            return EOF;
        for (i = 0; i < len; ++i)
            output[i] = str[i];
        return 0;
    }
}

static int ref_puts(strb_t *s, const char *str)
{
    return ref_nputs(s, str, SIZE_MAX);
}

#if !STRB_FREESTANDING
static int ref_vputf(strb_t *s, const char *format, va_list args)
{
    // Formatting is independent of strb; use the returned count so embedded
    // null characters are written too. All test output fits in this array.
    char output[128];
    int len = vsnprintf(output, sizeof output, format, args);
    assert(len >= 0);
    assert((size_t)len < sizeof output);
    {
        int i;
        _Optional char *destination = strb_write(s, (size_t)len);
        if (!destination)
            return EOF;
        for (i = 0; i < len; ++i)
            destination[i] = output[i];
#if STRB_RESTORE
        // Exercise the direct writer's terminating null and boundary repair.
        destination[len] = '\0';
        strb_restore(s);
#endif
        return 0;
    }
}

static int ref_putf(strb_t *s, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    {
        int result = ref_vputf(s, format, args);
        va_end(args);
        return result;
    }
}

static int call_strb_vputf(strb_t *s, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    {
        int result = strb_vputf(s, format, args);
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

static int put_library_output(strb_t *s, const output_case *testcase)
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

static int put_reference_output(strb_t *s, const output_case *testcase)
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

static void compare_output(const strb_t *actual, const strb_t *reference,
                           const output_case *testcase)
{
    bool equal = strb_len(actual) == strb_len(reference) &&
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

static void prepare_output(strb_t *s, const char *initial, size_t pos,
                           int mode, bool pending_undo, bool error)
{
    assert(!ref_puts(s, initial));
    assert(!strb_setmode(s, mode));
    strb_seek(s, pos);
    if (pending_undo)
        assert(strb_putc(s, 'q') == 'q');
    if (error) {
        assert(strb_setmode(s, -1) == EOF);
        assert(strb_error(s));
    }
}

static void check_output(strb_t *actual, strb_t *reference,
                         const output_case *testcase)
{
    int expected = put_reference_output(reference, testcase);
    assert(expected != EOF);
    assert(put_library_output(actual, testcase) == expected);
    compare_output(actual, reference, testcase);
#if STRB_RESTORE
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
#if STRB_UNPUTC
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
    const unsigned initial_states[] = {
        0, HAS_PENDING_UNDO, HAS_ERROR, HAS_PENDING_UNDO | HAS_ERROR,
#if STRB_RESTORE
        HAS_PENDING_RESTORE, HAS_PENDING_RESTORE | HAS_PENDING_UNDO,
        HAS_PENDING_RESTORE | HAS_ERROR,
        HAS_PENDING_RESTORE | HAS_PENDING_UNDO | HAS_ERROR,
#endif
    };
    const char *const initial_strings[] = {"", "abcdef"};
    const int modes[] = {strb_insert, strb_overwrite};
    for (t = 0; t < ARRAY_SIZE(output_cases); ++t) {
        size_t i;
        for (i = 0; i < ARRAY_SIZE(initial_strings); ++i) {
            size_t pos;
            const char *initial = initial_strings[i];
            for (pos = 0; pos <= strlen(initial) + 2; ++pos) {
                size_t m;
                for (m = 0; m < ARRAY_SIZE(modes); ++m) {
                    size_t state_index;
                    for (state_index = 0;
                         state_index < ARRAY_SIZE(initial_states); ++state_index) {
                        const unsigned flags = initial_states[state_index];
                        static char actual_array[128], reference_array[128];
#if STRB_EXT_STATE
                        strbstate_t actual_state, reference_state;
                        strb_t *actual = strb_use(&actual_state, sizeof actual_array,
                                                  actual_array);
                        strb_t *reference = strb_use(&reference_state,
                                                     sizeof reference_array,
                                                     reference_array);
#else
                        _Optional strb_t *actual = strb_use(sizeof actual_array,
                                                            actual_array);
                        _Optional strb_t *reference = strb_use(sizeof reference_array,
                                                               reference_array);
#endif
                        assert(actual);
                        assert(reference);
                        prepare_output(&*actual, initial, pos, modes[m],
                                       flags & HAS_PENDING_UNDO, flags & HAS_ERROR);
                        prepare_output(&*reference, initial, pos, modes[m],
                                       flags & HAS_PENDING_UNDO, flags & HAS_ERROR);
#if STRB_RESTORE
                        if (flags & HAS_PENDING_RESTORE) {
                            assert(!strb_split(&*actual));
                            assert(!strb_split(&*reference));
                        }
#endif
                        check_output(&*actual, &*reference, &output_cases[t]);
#if !STRB_FREESTANDING
                        strb_free(actual);
                        strb_free(reference);
#endif
#if !STRB_STATIC_ALLOC && !STRB_FREESTANDING
                        // Exercise owned storage as well as external arrays.
                        {
                            _Optional strb_t *owned_actual = strb_alloc(10);
                            _Optional strb_t *owned_reference = strb_alloc(10);
                            assert(owned_actual);
                            assert(owned_reference);
                            prepare_output(&*owned_actual, initial, pos, modes[m],
                                           flags & HAS_PENDING_UNDO, flags & HAS_ERROR);
                            prepare_output(&*owned_reference, initial, pos, modes[m],
                                           flags & HAS_PENDING_UNDO, flags & HAS_ERROR);
#if STRB_RESTORE
                            if (flags & HAS_PENDING_RESTORE) {
                                assert(!strb_split(&*owned_actual));
                                assert(!strb_split(&*owned_reference));
                            }
#endif
                            check_output(&*owned_actual, &*owned_reference,
                                         &output_cases[t]);
                            strb_free(owned_actual);
                            strb_free(owned_reference);
                        }
#endif
                    }
                }
            }
        }
    }
}

#if STRB_UNPUTC
static void test_write_zero(void)
{
    char array[128];
#if STRB_EXT_STATE
    strbstate_t state;
    strb_t *s = strb_use(&state, sizeof array, array);
#else
    _Optional strb_t *s = strb_use(sizeof array, array);
#endif
    assert(s);
    prepare_output(&*s, "abc", 0, strb_overwrite, true, false);
    assert(strb_write(&*s, 0));
    assert(strb_tell(&*s) == 1);
    assert(strb_len(&*s) == 3);
    assert(strb_unputc(&*s) == 'q');
    assert(strb_tell(&*s) == 0);
    assert(!strcmp(strb_cptr(&*s), "abc"));
#if !STRB_FREESTANDING
    strb_free(s);
#endif
}
#endif

#if !STRB_STATIC_ALLOC && !STRB_FREESTANDING
static void test_output_growth(void)
{
    size_t i, t;
    char initial[STRB_DFL_SIZE];
    const int modes[] = {strb_insert, strb_overwrite};
    memset(initial, 'a', sizeof initial - 1);
    initial[sizeof initial - 1] = '\0';
    for (t = 0; t < ARRAY_SIZE(output_cases); ++t) {
        size_t m;
        for (m = 0; m < ARRAY_SIZE(modes); ++m) {
            _Optional strb_t *actual = strb_alloc(STRB_DFL_SIZE);
            _Optional strb_t *reference = strb_alloc(STRB_DFL_SIZE);
            assert(actual);
            assert(reference);
            // Nonempty output at the end must exceed the initial capacity.
            prepare_output(&*actual, initial, strlen(initial), modes[m],
                           false, false);
            prepare_output(&*reference, initial, strlen(initial), modes[m],
                           false, false);
            check_output(&*actual, &*reference, &output_cases[t]);
            strb_free(actual);
            strb_free(reference);
        }
    }
    // Seeking beyond allocated storage is allowed; the write obtains storage.
    {
        _Optional strb_t *s = strb_alloc(0);
        const size_t pos = STRB_DFL_SIZE + sizeof "gap";
        assert(s);
        strb_seek(&*s, pos);
        assert(strb_tell(&*s) == pos);
        assert(strb_len(&*s) == 0);
        assert(!strb_error(&*s));
        assert(strb_putc(&*s, 'X') == 'X');
        assert(strb_tell(&*s) == pos + 1);
        assert(strb_len(&*s) == pos + 1);
        for (i = 0; i < pos; ++i)
            assert(strb_cptr(&*s)[i] == '\0');
        assert(strb_cptr(&*s)[pos] == 'X');
        assert(strb_cptr(&*s)[pos + 1] == '\0');
        strb_free(s);
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
    const int modes[] = {strb_insert, strb_overwrite};
    for (t = 0; t < ARRAY_SIZE(output_cases); ++t) {
        size_t pos;
        const output_case *testcase = &output_cases[t];
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
#if STRB_EXT_STATE
                strbstate_t actual_state, reference_state;
                strb_t *actual = strb_use(&actual_state, sizeof actual_array,
                                          actual_array);
                strb_t *reference = strb_use(&reference_state,
                                             sizeof reference_array,
                                             reference_array);
#else
                _Optional strb_t *actual = strb_use(sizeof actual_array,
                                                    actual_array);
                _Optional strb_t *reference = strb_use(sizeof reference_array,
                                                       reference_array);
#endif
                assert(actual);
                assert(reference);
                prepare_output(&*actual, "abcdef", pos, modes[m], true, false);
                prepare_output(&*reference, "abcdef", pos, modes[m], true, false);
                assert(put_library_output(&*actual, testcase) == EOF);
                assert(strb_error(&*actual));
                strb_clearerr(&*actual);
                compare_output(&*actual, &*reference, testcase);
#if STRB_UNPUTC
                // Failure must preserve the previously available undo.
                assert(strb_unputc(&*actual) == strb_unputc(&*reference));
                compare_output(&*actual, &*reference, testcase);
#endif
#if !STRB_FREESTANDING
                strb_free(actual);
                strb_free(reference);
#endif
            }
        }
    }
}

#endif // STRB_EXT_STATE || !STRB_FREESTANDING

#if STRB_EXT_STATE || !STRB_FREESTANDING
static void test_delto_large_target(void)
{
    size_t t;
    const size_t targets[] = {
        (size_t)STRB_MAX_SIZE - 1, STRB_MAX_SIZE,
        (size_t)STRB_MAX_SIZE + 1, (size_t)STRB_MAX_SIZE + 2, SIZE_MAX
    }, positions[] = {0, 3, 8};
    const int modes[] = {strb_insert, strb_overwrite};
    for (t = 0; t < ARRAY_SIZE(targets); ++t) {
        size_t p;
        for (p = 0; p < ARRAY_SIZE(positions); ++p) {
            size_t m;
            for (m = 0; m < ARRAY_SIZE(modes); ++m) {
                char array[128];
#if STRB_EXT_STATE
                strbstate_t state;
                strb_t *s = strb_use(&state, sizeof array, array);
#else
                _Optional strb_t *s = strb_use(sizeof array, array);
#endif
                assert(s);
                assert(!strb_puts(&*s, "abcdef"));
                assert(!strb_setmode(&*s, modes[m]));
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
                    strb_free(s);
#endif
                }
            }
        }
    }
}
#endif

#if STRB_EXT_STATE || !STRB_FREESTANDING
static void test_seek(void)
{
    size_t i, m;
    static const char initial[] = "abcdef";
    const size_t unsupported[] = {STRB_MAX_SIZE, (size_t)STRB_MAX_SIZE + 1, SIZE_MAX};
    const int modes[] = {strb_insert, strb_overwrite};
    char array[64];
#if STRB_EXT_STATE
    strbstate_t state;
    strb_t *s = strb_use(&state, sizeof array, array);
#else
    _Optional strb_t *s = strb_use(sizeof array, array);
#endif
    assert(s);
    for (m = 0; m < ARRAY_SIZE(modes); ++m) {
        size_t pos, u;
        assert(!strb_cpy(&*s, initial));
        assert(!strb_setmode(&*s, modes[m]));
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
#if STRB_UNPUTC
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
#if STRB_RESTORE
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
        strb_free(s);
#endif
    }
}
#endif

static void test(strb_t *const s)
{
    char *c;
    int i;
    char *found;
    size_t pos;

    for (i = 5; i >= 0; --i) {
        strb_seek(s, 0);
        assert(strb_putc(s, 'a' + i) == 'a' + i);
        assert(strb_ptr(s)[strb_len(s)] == '\0');
#if !STRB_FREESTANDING
        assert(!strb_putf(s, "fmt%dx", i));
        assert(strb_ptr(s)[strb_len(s)] == '\0');
#if STRB_UNPUTC
        assert(strb_unputc(s) == 'x');
#endif
#else
#if STRB_UNPUTC
        assert(strb_unputc(s) == 'a' + i);
#endif
#endif // !STRB_FREESTANDING
        assert(strb_ptr(s)[strb_len(s)] == '\0');
#if STRB_UNPUTC
        assert(strb_unputc(s) == EOF);
#endif
        assert(strb_ptr(s)[strb_len(s)] == '\0');
        assert(!strb_puts(s, "str"));
        assert(strb_ptr(s)[strb_len(s)] == '\0');
    }

    puts(strb_ptr(s));

    assert(!strb_setmode(s, strb_overwrite));
    assert(strb_getmode(s) == strb_overwrite);

    assert(!strb_puts(s, "OVERWRITE"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));

    strb_seek(s, strb_len(s) - 2);
    assert(!strb_puts(s, "OVERWRITE"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));

    assert(!strb_setmode(s, strb_insert));
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
        size_t lo = strlen("DELETEME") + 1, hi = lo + 1;

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
        _Optional char *w = strb_write(s, 0);
        assert(w);
        *w = '\0';
    }

    assert(!strcmp(strb_ptr(s), "R2"));
    assert(strb_len(s) == 4);
    assert(strb_tell(s) == 2);
    puts(strb_ptr(s));

#if STRB_RESTORE
    strb_restore(s);
    assert(!strcmp(strb_ptr(s), "R2D2"));
    assert(strb_len(s) == 4);
    assert(strb_tell(s) == 2);
    puts(strb_ptr(s));
#endif

    strb_seek(s, strb_len(s));
    {
        _Optional char *w = strb_write(s, 0);
        assert(w);
        *w = 'q'; // probably illegal!
    }
    assert(strb_ptr(s)[strb_len(s)] == 'q');

#if STRB_RESTORE
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

#if STRB_RESTORE
    strb_restore(s);
    assert(!strcmp(strb_ptr(s), "C3P0"));
    assert(strb_len(s) == 4);
    assert(strb_tell(s) == 2);
    puts(strb_ptr(s));
#endif

    strb_seek(s, strb_len(s));
    assert(!strb_split(s));
    assert(strb_ptr(s)[strb_len(s)] == '\0');

#if STRB_RESTORE
    strb_restore(s);
    assert(strb_ptr(s)[strb_len(s)] == '\0');
#endif

#endif // !STRB_FREESTANDING

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    {
        strb_t const *sc = s;
        const char *q = strb_ptr(sc);
        assert(strb_ptr(sc)[strb_len(sc)] == '\0');
        puts(q);
    }
#endif

    {
        strb_t const *sc = s;
        const char *q = strb_cptr(sc);
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

int main(void)
{
    static char array[1000];
    _Optional strb_t *s;
    int c;

#if STRB_EXT_STATE
    strbstate_t state;
#endif

#if STRB_EXT_STATE || !STRB_FREESTANDING
    test_output_equivalence();
#if STRB_UNPUTC
    test_write_zero();
#endif
#if !STRB_STATIC_ALLOC && !STRB_FREESTANDING
    test_output_growth();
#endif
    test_output_failure();
    test_delto_large_target();
    test_seek();
#endif

#if STRB_EXT_STATE
    s = strb_use(&state, sizeof array, array);
    if (!s) {
        assert(s);
        return 1;
    }
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    test(&*s);
    c = strb_ptr(&*s)[strb_len(&*s) - 1];

    s = strb_reuse(&state, sizeof array, array);
    if (!s) {
        assert(s);
        return 1;
    }
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == c);
#else
    (void)c;
#endif

    test(&*s);

    memset(array, 'a', sizeof array);
    // No null terminator.
    assert(strb_reuse(&state, sizeof array, array) == NULL);

    strcpy(array,
             "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
             "Curabitur lacinia mi "
             "mollis, tincidunt ipsum ut, commodo massa. Maecenas sit amet "
             "mattis augue. Fusce "
             "bibendum condimentum tortor accumsan sodales. Curabitur "
             "accumsan, ante sit amet "
             "commodo massa nunc. ");
    s = strb_reuse(&state, sizeof array, array);
#if STRB_MAX_SIZE <= UINT8_MAX
    assert(!s); // too long
#else
    assert(s);
    puts(strb_ptr(&*s));
#endif

#if STRB_REUSE_CONST
    {
        _Optional const strb_t *cs = strb_reuse_const(&state, "Cyclist");
        if (!cs) {
            assert(cs);
            return 1;
        }
        assert(strb_getmode(&*cs) == strb_insert);
        assert(strb_tell(&*cs) == strlen("Cyclist"));
        assert(strb_len(&*cs) == strlen("Cyclist"));
        assert(!strcmp(strb_cptr(&*cs), "Cyclist"));
        puts(strb_cptr(&*cs));
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
        puts(strb_ptr(&*cs));
#endif

        cs = strb_reuse_const(
            &state,
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
            "Curabitur "
            "lacinia mi mollis, tincidunt ipsum ut, commodo massa. Maecenas "
            "sit "
            "amet mattis augue. Fusce bibendum condimentum tortor accumsan "
            "sodales. Curabitur accumsan, ante sit amet commodo massa nunc. ");
#if STRB_MAX_SIZE <= UINT8_MAX
        assert(!cs); // too long
#else
        assert(cs);
        puts(strb_cptr(&*cs));
#endif
    }
#endif // STRB_REUSE_CONST

#elif !STRB_FREESTANDING
    s = strb_use(sizeof array, array);
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    test(&*s);
    c = strb_ptr(&*s)[strb_len(s) - 1];
    strb_free(s);

    s = strb_reuse(sizeof array, array);
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == c);
#else
    (void)c;
#endif

    test(&*s);
    strb_free(s);

    memset(array, 'a', sizeof array);
    assert(strb_reuse(sizeof array, array) == NULL);
#else
    (void)s;
    (void)array;
    (void)test;
    (void)c;
#endif // !STRB_FREESTANDING

#if !STRB_FREESTANDING
    s = strb_alloc(2700);
    assert(s);
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    test(&*s);
    strb_free(s);

    s = strb_alloc(5);
    assert(s);
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    test(&*s);
    strb_free(s);

    s = strb_alloc(5000);
    assert(s);
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    strb_write(&*s, 0);
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    test(&*s);
    strb_free(s);

    s = strb_ndup("", 0);
    assert(s);
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif
    strb_free(s);

    s = strb_dup("");
    assert(s);
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    assert(!strb_cpy(&*s, ""));
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    assert(!strb_ncpy(&*s, "", 0));
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    strb_free(s);

    s = strb_aprintf("");
    assert(s);
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif
    assert(!strb_printf(&*s, ""));
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == EOF);
#endif

    strb_free(s);

    s = strb_dup("DUPLICATE");
    assert(s);
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == 'E');
    assert(strb_unputc(&*s) == EOF);
#endif
    test(&*s);
    strb_free(s);

    s = strb_ndup("DUPLICATE", 3);
    assert(s);
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == 'P');
    assert(strb_unputc(&*s) == EOF);
#endif

    test(&*s);
    strb_free(s);

    s = strb_aprintf("Hello %d", 99);
    assert(s);
#if STRB_UNPUTC
    assert(strb_unputc(&*s) == '9');
    assert(strb_unputc(&*s) == EOF);
#endif

    test(&*s);
    strb_free(s);

    s = strb_dup("Lorem ipsum dolor sit amet");
    assert(s);
    puts(strb_ptr(&*s));
    strb_free(s);

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
    strb_free(s);

#endif // !STRB_FREESTANDING
    return 0;
}
