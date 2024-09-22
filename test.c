// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "strb.h"

static void test(strb_t *s)
{
    int i;
    char *found;
    size_t pos;
    
    if (!s) return;

    for ( i = 5; i >= 0; --i) {
        assert(!strb_seek(s, 0));
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

    assert(!strb_seek(s, strb_len(s) - 2));
    assert(!strb_puts(s, "OVERWRITE"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));

    assert(!strb_setmode(s, strb_insert));
    assert(strb_getmode(s) == strb_insert);

    found = strstr(strb_ptr(s), "fmt4");
    if (found) {
        assert(!strb_seek(s, (size_t)(found - strb_ptr(s))));
        printf("%zu\n", strb_tell(s));
        assert(!strb_puts(s, "INSERT"));
        assert(strb_ptr(s)[strb_len(s)] == '\0');
        puts(strb_ptr(s));
    }

    assert(!strb_seek(s, strb_len(s) + 2));
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

        assert(!strb_seek(s, lo));
        assert(strb_tell(s) == lo);
        strb_delto(s, hi); // no-op
        assert(strb_tell(s) == lo);

        assert(!strb_seek(s, hi));
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

    assert(!strb_seek(s, strlen("DELETEM")));
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

    assert(!strb_seek(s, strlen("D")));
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

    assert(!strb_seek(s, 2));
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

    assert(!strb_seek(s, strb_len(s)));
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

    assert(!strb_seek(s, 2));
    strb_split(s);
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

    assert(!strb_seek(s, strb_len(s)));
    strb_split(s);
    assert(strb_ptr(s)[strb_len(s)] == '\0');

#if STRB_RESTORE
    strb_restore(s);
    assert(strb_ptr(s)[strb_len(s)] == '\0');
#endif

#endif // !STRB_FREESTANDING

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    {
        strb_t const *sc = s;
        assert(strb_ptr(sc)[strb_len(sc)] == '\0');
        const char *q = strb_ptr(sc);
        puts(q);
    }
#endif

    {
        strb_t const *sc = s;
        assert(strb_cptr(sc)[strb_len(sc)] == '\0');
        const char *q = strb_cptr(sc);
        puts(q);
    }

    for (char *c = strb_ptr(s);
        *c != '\0';
        ++c)
    {
        *c = tolower(*c);
    }
    puts(strb_cptr(s));

    for (char *c = strb_ptr(s);
        *c != '\0';
        ++c)
    {
        *c = toupper(*c);
    }
    puts(strb_cptr(s));

    puts("========");
}

int main(void)
{
    char array[1000];
    _Optional strb_t *s;
    int c;

#if STRB_EXT_STATE
    strbstate_t state;

    s = strb_use(&state, sizeof array, array);
#if STRB_UNPUTC
    assert(strb_unputc(s) == EOF);
#endif

    test(s);
    c = strb_ptr(s)[strb_len(s) - 1];

    s = strb_reuse(&state, sizeof array, array);
#if STRB_UNPUTC
    assert(strb_unputc(s) == c);
#else
    (void)c;
#endif

    test(s);

    memset(array, 'a', sizeof array);
    assert(strb_reuse(&state, sizeof array, array) == NULL); // no null terminator

    snprintf(array, sizeof array, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur lacinia mi mollis, tincidunt ipsum ut, commodo massa. Maecenas sit amet mattis augue. Fusce bibendum condimentum tortor accumsan sodales. Curabitur accumsan, ante sit amet commodo massa nunc. ");
    s = strb_reuse(&state, sizeof array, array);
#if STRB_MAX_SIZE <= UINT8_MAX
    assert(!s); // too long
#else
    puts(strb_ptr(s));
#endif

#if STRB_REUSE_CONST
    {
        _Optional const strb_t *cs = strb_reuse_const(&state, "Cyclist");
        assert(strb_getmode(cs) == strb_insert);
        assert(strb_tell(cs) == strlen("Cyclist"));
        assert(strb_len(cs) == strlen("Cyclist"));
        assert(!strcmp(strb_cptr(cs), "Cyclist"));
        puts(strb_cptr(cs));
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
        puts(strb_ptr(cs));
#endif

        cs = strb_reuse_const(&state, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur lacinia mi mollis, tincidunt ipsum ut, commodo massa. Maecenas sit amet mattis augue. Fusce bibendum condimentum tortor accumsan sodales. Curabitur accumsan, ante sit amet commodo massa nunc. ");
#if STRB_MAX_SIZE <= UINT8_MAX
        assert(!cs); // too long
#else
        puts(strb_cptr(cs));
#endif
    }
#endif // STRB_REUSE_CONST

#elif !STRB_FREESTANDING
    s = strb_use(sizeof array, array);
#if STRB_UNPUTC
    assert(strb_unputc(s) == EOF);
#endif

    test(s);
    c = strb_ptr(s)[strb_len(s) - 1];
    strb_free(s);

    s = strb_reuse(sizeof array, array);
#if STRB_UNPUTC
    assert(strb_unputc(s) == c);
#else
    (void)c;
#endif

    test(s);
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
#if STRB_UNPUTC
    assert(strb_unputc(s) == EOF);
#endif

    test(s);
    strb_free(s);

    s = strb_alloc(5);
#if STRB_UNPUTC
    assert(strb_unputc(s) == EOF);
#endif

    test(s);
    strb_free(s);

    s = strb_alloc(5000);
#if STRB_UNPUTC
    assert(strb_unputc(s) == EOF);
#endif

    strb_write(s, 0);
#if STRB_UNPUTC
    assert(strb_unputc(s) == EOF);
#endif

    test(s);
    strb_free(s);

    s = strb_ndup("", 0);
#if STRB_UNPUTC
    assert(strb_unputc(s) == EOF);
#endif
    strb_free(s);

    s = strb_dup("");
#if STRB_UNPUTC
    assert(strb_unputc(s) == EOF);
#endif

    assert(!strb_cpy(s, ""));
#if STRB_UNPUTC
    assert(strb_unputc(s) == EOF);
#endif

    assert(!strb_ncpy(s, "", 0));
#if STRB_UNPUTC
    assert(strb_unputc(s) == EOF);
#endif

    strb_free(s);

    s = strb_aprintf("");
#if STRB_UNPUTC
    assert(strb_unputc(s) == EOF);
#endif
    assert(!strb_printf(s, ""));
#if STRB_UNPUTC
    assert(strb_unputc(s) == EOF);
#endif

    strb_free(s);

    s = strb_dup("DUPLICATE");
#if STRB_UNPUTC
    assert(strb_unputc(s) == 'E');
    assert(strb_unputc(s) == EOF);
#endif
    test(s);
    strb_free(s);

    s = strb_ndup("DUPLICATE", 3);
#if STRB_UNPUTC
    assert(strb_unputc(s) == 'P');
    assert(strb_unputc(s) == EOF);
#endif

    test(s);
    strb_free(s);

    s = strb_aprintf("Hello %d", 99);
#if STRB_UNPUTC
    assert(strb_unputc(s) == '9');
    assert(strb_unputc(s) == EOF);
#endif

    test(s);
    strb_free(s);

    s = strb_dup("Lorem ipsum dolor sit amet");
    puts(strb_ptr(s));
    strb_free(s);

    s = strb_dup("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur lacinia mi mollis, tincidunt ipsum ut, commodo massa. Maecenas sit amet mattis augue. Fusce bibendum condimentum tortor accumsan sodales. Curabitur accumsan, ante sit amet commodo massa nunc. ");
#if STRB_MAX_SIZE <= UINT8_MAX
    assert(!s); // too long
#else
   puts(strb_ptr(s));
#endif
    strb_free(s);

#endif // !STRB_FREESTANDING
    return 0;
}
