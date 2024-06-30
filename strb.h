#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

#if TINIER
// No static or dynamic allocation
#define STRB_MAX SIZE_MAX
typedef uint8_t strbsize_t;
#define PRIstrbsize PRIu8
#define STRB_MAX_SIZE UINT8_MAX
#define STRB_SIZE_HINT(X)

#elif TINY
// About 2KB of static storage
#define STRB_MAX (8) // must not exceed 8
typedef uint8_t strbsize_t;
#define PRIstrbsize PRIu8
#define STRB_MAX_SIZE (256-8)
#define STRB_MAX_INTERNAL_SIZE STRB_MAX_SIZE
#define STRB_SIZE_HINT(X)

#else
// Unlimited dynamic allocation
#define STRB_MAX SIZE_MAX
typedef uint16_t strbsize_t;
#define PRIstrbsize PRIu16
#define STRB_MAX_SIZE UINT16_MAX
#define STRB_DFL_SIZE (256)
#define STRB_MAX_INTERNAL_SIZE (256)
#define STRB_SIZE_HINT(X) (X)
#define STRB_GROW_FACTOR (2)

#endif

#define _Optional

typedef struct strb_t strb_t;

typedef struct {
    char restore_char, write_char, flags;
    strbsize_t len, size, pos;
    char *buf;
} strbprivate_t;

typedef struct  {
    strbprivate_t p;
} strbstate_t;

strb_t *strbstate_use(strbstate_t *sbs, size_t size, char buf[STRB_SIZE_HINT(size)]);
_Optional strb_t *strbstate_reuse(strbstate_t *sbs, size_t size, char buf[STRB_SIZE_HINT(size)]);

_Optional strb_t *strb_alloc(size_t size);
_Optional strb_t *strb_use(size_t size, char buf[STRB_SIZE_HINT(size)]);
_Optional strb_t *strb_reuse(size_t size, char buf[STRB_SIZE_HINT(size)]);

_Optional strb_t *strb_dup(const char *str);
_Optional strb_t *strb_ndup(const char *str, size_t n);

_Optional strb_t *strb_aprintf(const char *format, ...);
_Optional strb_t *strb_vaprintf(const char *format, va_list args );

void strb_free(_Optional strb_t *sb );

const char *strb_ptr(strb_t const *sb );
size_t strb_len(strb_t const *sb );

enum {
  strb_insert,
  strb_overwrite
};

int strb_setmode(strb_t *sb, int mode);
int strb_getmode(const strb_t *sb );

int strb_seek(strb_t *sb, size_t pos);
size_t strb_tell(strb_t const *sb );

int strb_putc(strb_t *sb, int c); 
int strb_nputc(strb_t *sb, int c, size_t n);
int strb_unputc(strb_t *sb);
int strb_puts(strb_t *sb, const char *str );
int strb_nputs(strb_t *sb, const char *str, size_t n);

int strb_vputf(strb_t *sb, const char *format, va_list args );
int strb_putf(strb_t *sb, const char *format, ...);

_Optional char *strb_write(strb_t *sb, size_t n);
void strb_wrote(strb_t *sb);

void strb_delto(strb_t *sb, size_t pos);

int strb_cpy(strb_t *sb, const char *str );
int strb_ncpy(strb_t *sb, const char *str, size_t n);

int strb_vprintf(strb_t *sb, const char *format, va_list args );
int strb_printf(strb_t *sb, const char *format, ...);

bool strb_error(strb_t const *sb );
void strb_clearerr(strb_t *sb);
