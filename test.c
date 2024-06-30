#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

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
        assert(!strb_putf(s, "fmt%dx", i));
        assert(strb_ptr(s)[strb_len(s)] == '\0');
        assert(strb_unputc(s) == 'x');
        assert(strb_ptr(s)[strb_len(s)] == '\0');
        assert(strb_unputc(s) == EOF);
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
    assert(strb_len(s) == 0);
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));

    assert(!strb_puts(s, "DELETEME"));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    assert(!strcmp(strb_ptr(s), "DELETEME"));
    assert(strb_len(s) == 8);

    strb_delto(s, strb_tell(s)); // no-op
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "DELETEME"));
    assert(strb_len(s) == 8);

    strb_delto(s, SIZE_MAX); // no-op
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "DELETEME"));
    assert(strb_len(s) == 8);

    assert(!strb_seek(s, strb_tell(s) - 2));
    strb_delto(s, SIZE_MAX); // delete "ME"
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "DELETE"));
    assert(strb_len(s) == 6);

    strb_delto(s, strb_tell(s) - 3); // delete "ETE"
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "DEL"));
    assert(strb_len(s) == 3);

    assert(!strb_seek(s, 1));
    strb_delto(s, 2); // delete "E"
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "DL"));
    assert(strb_len(s) == 2);

    strb_delto(s, 0); // delete "D"
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "L"));
    assert(strb_len(s) == 1);

    assert(!strb_puts(s, "FEE")); // make "FEEL"
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    puts(strb_ptr(s));
    assert(!strcmp(strb_ptr(s), "FEEL"));
    assert(strb_len(s) == 4);

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

    assert(!strb_printf(s, "R%dD%d", 2, 2));
    assert(strb_ptr(s)[strb_len(s)] == '\0');
    assert(!strcmp(strb_ptr(s), "R2D2"));
    assert(strb_len(s) == 4);
    puts(strb_ptr(s));

    puts("========");
}

int main(void)
{
    strbstate_t state;
    char array[100];
    strb_t *s;
    int c;

    s = strbstate_use(&state, sizeof array, array);
    c = strb_unputc(s);
    assert(c == EOF);

    test(s);

    assert(strbstate_reuse(&state, 0, array) == NULL);

    s = strbstate_reuse(&state, sizeof array, array);
    c = strb_unputc(s);
    assert(c == EOF);

    test(s);

    memset(array, 'a', sizeof array);
    assert(strbstate_reuse(&state, 0, array) == NULL);

#if !TINIER
    assert(strb_use(0, array) == NULL);

    s = strb_use(sizeof array, array);
    c = strb_unputc(s);
    assert(c == EOF);

    test(s);
    strb_free(s);

    assert(strb_reuse(0, array) == NULL);

    s = strb_reuse(sizeof array, array);
    c = strb_unputc(s);
    assert(c == EOF);

    test(s);
    strb_free(s);

    memset(array, 'a', sizeof array);
    assert(strb_reuse(0, array) == NULL);

    s = strb_alloc(2700);
    c = strb_unputc(s);
    assert(c == EOF);

    test(s);
    strb_free(s);

    s = strb_alloc(5);
    c = strb_unputc(s);
    assert(c == EOF);

    test(s);
    strb_free(s);

    s = strb_alloc(5000);
    c = strb_unputc(s);
    assert(c == EOF);

    test(s);
    strb_free(s);

    s = strb_dup("DUPLICATE");
    c = strb_unputc(s);
    assert(c == 'E');
    c = strb_unputc(s);
    assert(c == EOF);

    test(s);
    strb_free(s);

    s = strb_ndup("DUPLICATE", 3);
    c = strb_unputc(s);
    assert(c == 'P');
    c = strb_unputc(s);
    assert(c == EOF);

    test(s);
    strb_free(s);

    s = strb_aprintf("Hello %d", 99);
    c = strb_unputc(s);
    assert(c == '9');
    c = strb_unputc(s);
    assert(c == EOF);

    test(s);
    strb_free(s);

    s = strb_dup("Lorem ipsum dolor sit amet");
    puts(strb_ptr(s));
    strb_free(s);

    s = strb_dup("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur lacinia mi mollis, tincidunt ipsum ut, commodo massa. Maecenas sit amet mattis augue. Fusce bibendum condimentum tortor accumsan sodales. Curabitur accumsan, ante sit amet commodo massa nunc. ");
#if !TINY
    puts(strb_ptr(s));
#else
    assert(!s);
#endif
    strb_free(s);

#endif
    return 0;
}
