// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

#define STRB_EXT_STATE 1

#if STRB_FREESTANDING
// No static or dynamic allocation
#define STRB_MAX SIZE_MAX
typedef uint8_t strbsize_t;
#define PRIstrbsize PRIu8
#define STRB_MAX_SIZE UINT8_MAX
#define STRB_SIZE_HINT(X)

#elif STRB_STATIC_ALLOC
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

/**
 * @brief String buffer object
 *
 * An object type to be used in place of strbstate_t for all operations on a string buffer. It
 * need not be a complete type. Consequently, it may be impossible to wrongly declare an object of
 * type strb_t and its internal state should only be accessed by the provided functions.
 */
typedef struct strb_t strb_t;

/**
 * @private
 */
typedef struct {
    char restore_char, write_char, flags;
    strbsize_t len, size, pos;
    char *buf;
} strbprivate_t;

#if STRB_EXT_STATE

/**
 * @brief String buffer state
 *
 * An object type capable of recording all the information needed to control a string
 * buffer, including a position indicator, the buffer’s size, the length of the string in the buffer, and an
 * error indicator. It is a complete type so it can be used for storage allocation, but its members are
 * unspecified.
 *
 * An object of type strbstate_t can only be used to construct strb_t objects. Its storage duration
 * must exceed the program’s usage of any strb_t pointers derived from it. The effect on any derived
 * strb_t object of modifying the originating strbstate_t object is undefined.
 */
typedef struct  {
    /**
     * @private
     */
    strbprivate_t p;
} strbstate_t;

/**
 * @brief Create a string buffer object for managing an external character array
 *
 * Initialises a string buffer object and returns its address. The caller must pass a
 * string buffer state object to be used to store information about the buffer.
 *
 * A null character is written as the first character of @p buf.
 * The initial string length is 0.
 *
 * The effects of strb_... functions on an external array are always immediately visible
 * and the string therein is always null terminated.
 *
 * @param[out] sbs   String buffer state.
 * @param[out] buf   A character array to be used instead of an internal buffer.
 * @param      size  Size of the array (which must not be 0).
 *
 * @return Address of the created string buffer object.
 * @post The user may call @ref strb_free to free the string buffer object, but
 *       it is not required and has no effect.
 * @post The created string buffer object becomes invalid if the storage for @p sbs or @p buf
 *       is deallocated.
 * @post The created string buffer object becomes invalid if the @p sbs or @p buf object is
 *       modified (other than as a side-effect of calling a strb_... function which is not
 *       @ref strb_reuse or @ref strb_use).
 */
strb_t *strb_use(strbstate_t *sbs, size_t size, char buf[STRB_SIZE_HINT(size)]);

/**
 * @brief Create a string buffer object by reusing an external character array
 *
 * Initialises a string buffer object and returns its address. The caller must pass a
 * string buffer state object to be used to store information about the buffer.
 *
 * The @p buf array should contain at least one null character within the first @p size
 * characters, whose position is the initial string length. Any following characters are ignored.
 * If no null character is found within the first @p size characters (or the maximum supported
 * length, if less), a null pointer is returned.
 *
 * The effects of strb_... functions on an external array are always immediately visible
 * and the string therein is always null terminated.
 *
 * @param[out]    sbs   String buffer state.
 * @param[in,out] buf   A character array to be used instead of an internal buffer.
 * @param         size  Size of the array (which must not be 0).
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user may call @ref strb_free to free the string buffer object, but
 *       it is not required and has no effect.
 * @post The created string buffer object becomes invalid if the storage for @p sbs or @p buf
 *       is deallocated.
 * @post The created string buffer object becomes invalid if the @p sbs or @p buf object is
 *       modified (other than as a side-effect of calling a strb_... function which is not
 *       @ref strb_reuse or @ref strb_use).
 */
_Optional strb_t *strb_reuse(strbstate_t *sbs, size_t size, char buf[STRB_SIZE_HINT(size)]);

#elif !STRB_FREESTANDING


/**
 * @brief Create a string buffer object for managing an external character array
 *
 * Allocates storage for a string buffer object, initialises it, and returns its address.
 * If storage allocation fails, a null pointer is returned.
 *
 * A null character is written as the first character of @p buf.
 * The initial string length is 0.
 *
 * The effects of strb_... functions on an external array are always immediately visible
 * and the string therein is always null terminated.
 *
 * @param[out] buf   A character array to be used instead of an internal buffer.
 * @param      size  Size of the array (which must not be 0).
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object. 
 */
_Optional strb_t *strb_use(size_t size, char buf[STRB_SIZE_HINT(size)]);

/**
 * @brief Create a string buffer object by reusing an external character array
 *
 * Allocates storage for a string buffer object, initialises it, and returns its address.
 * If storage allocation fails, a null pointer is returned.
 *
 * The @p buf array should contain at least one null character within the first @p size
 * characters, whose position is the initial string length. Any following characters are ignored.
 * If no null character is found within the first @p size characters (or the maximum supported
 * length, if less), a null pointer is returned.
 *
 * The effects of strb_... functions on an external array are always immediately visible
 * and the string therein is always null terminated.
 *
 * @param[in,out] buf   A character array to be used instead of an internal buffer.
 * @param         size  Size of the array (which must not be 0).
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object. 
 */
_Optional strb_t *strb_reuse(size_t size, char buf[STRB_SIZE_HINT(size)]);

#endif

#if !STRB_FREESTANDING
/**
 * @brief Create a string buffer object with internal storage
 *
 * Allocates storage for a string buffer object, initialises it, and returns its address.
 * If storage allocation fails, a null pointer is returned.
 *
 * The initial string length is 0.
 *
 * @param size  A hint about how much storage to allocate for an internal buffer
 *              (where 0 means default size).
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object. 
 */
_Optional strb_t *strb_alloc(size_t size);

/**
 * @brief Create a string buffer object with internal storage by duplicating a string
 *
 * Allocates storage for a string buffer object, initialises it, and returns its address.
 * If storage allocation fails, a null pointer is returned.
 *
 * Copies the given string into internal storage (if any), including its null terminator.
 *
 * @param[in] str   A null terminated string to be copied as the initial value of
 *                  the buffer.
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object. 
 */
_Optional strb_t *strb_dup(const char *str);

/**
 * @brief Create a string buffer object with internal storage by duplicating a sequence of characters
 *
 * Allocates storage for a string buffer object, initialises it, and returns its address.
 * If storage allocation fails, a null pointer is returned.
 *
 * Copies up to @p n characters from the array at @p str into internal storage, then
 * appends a null terminator. A null character and any characters following it are not copied.
 *
 * @param[in] str   An array of characters to be copied as the initial value of the buffer.
 * @param     n     Maximum number of characters to copy from @p str.
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object. 
 */
_Optional strb_t *strb_ndup(const char *str, size_t n);

/**
 * @brief Create a string buffer object with internal storage by parsing a format string.
 *
 * Allocates storage for a string buffer object, initialises it, and returns its address.
 * If storage allocation fails, a null pointer is returned.
 *
 * The @p format parameter specifies how to convert subsequent arguments to generate a string,
 * which forms the initial value of the buffer.
 *
 * @param[in] format   Specifies how to convert subsequent arguments to generate a string.
 * @param     ...      Arguments to be substituted into the generated string.
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object. 
 */
_Optional strb_t *strb_aprintf(const char *format, ...);

/**
 * @brief Create a string buffer object with internal storage by parsing a format string.
 *
 * Allocates storage for a string buffer object, initialises it, and returns its address.
 * If storage allocation fails, a null pointer is returned.
 *
 * The @p format parameter specifies how to convert subsequent arguments to generate a string,
 * which forms the initial value of the buffer.
 *
 * @param[in] format   Specifies how to convert subsequent arguments to generate a string.
 * @param     args     Variable argument list to be substituted into the generated string.
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object. 
 */
_Optional strb_t *strb_vaprintf(const char *format, va_list args);

/**
 * @brief Destroy a string buffer object.
 *
 * The associated buffer is also automatically freed, except in the case where its address
 * was passed as a parameter to @ref strb_use or @ref strb_reuse. This must be enforced
 * because storage allocation is abstracted. To pass an internally allocated string to code
 * that needs to take ownership of it, it must first be copied (e.g., using @c strdup).
 *
 * May be called with a null pointer, in which case this function has no effect.
 *
 * @param[in] sb  String buffer to destroy, or a null pointer.
 * @pre  The given @p sb address is null or was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @p strb_dup, @p strb_ndup, @p strb_aprintf or @p strb_vaprintf.
 * @post @p sb is invalid for use with any function.
 */
void strb_free(_Optional strb_t *sb);
#endif

/**
 * @brief Get a pointer to the character array underlying a string buffer.
 *
 * The returned pointer is guaranteed to be usable as a string (i.e. null terminated).
 *
 * @param[in] sb  String buffer.
 * @return Address of the first character stored in the string buffer.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @p strb_dup, @p strb_ndup, @p strb_aprintf or @p strb_vaprintf.
 * @post The returned pointer is valid until the next call to a strb_... function.
 */
const char *strb_ptr(strb_t const *sb);

/**
 * @brief Get the number of characters stored in a string buffer.
 *
 * A null character may appear anywhere in string buffer, since it is easy to insert
 * one by calling @c strb_putf(sb, "%c", 0). It would be hard to prevent such misuse.
 * Consequently, @c strb_len(sb) and @c strlen(strb_ptr(sb)) can return different results.
 * The returned character count is never less than the true string length, but may be greater.
 *
 * @param[in] sb  String buffer.
 * @return Number of characters stored in the string buffer.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @p strb_dup, @p strb_ndup, @p strb_aprintf or @p strb_vaprintf.
 */
size_t strb_len(strb_t const *sb);

/**
 * @brief Editing mode.
 */
enum {
  strb_insert,
  strb_overwrite
};

/**
 * @brief Set editing mode.
 *
 * Instead of providing variants of every function to insert or overwrite characters, this behaviour is
 * controlled by a mode stored in the string buffer object. The initial mode is @ref strb_insert.
 *
 * Changing the mode can fail if the requested mode is not supported by the library, in which case the
 * current mode is unchanged. Any value returned by @ref strb_getmode is accepted by @ref strb_setmode.
 *
 * @param[in,out] sb  String buffer.
 * @param         mode New mode.
 * @return If successful, zero, otherwise @c EOF.
 * @post Alters the behaviour of @ref strb_putc, @ref strb_unputc, @ref strb_puts, @ref strb_nputs,
 *       @ref strb_vputf, @ref strb_putf, @ref strb_write and @ref strb_delto.
 */
int strb_setmode(strb_t *sb, int mode);

/**
 * @brief Get editing mode.
 *
 * @param[in] sb  String buffer.
 * @return Current editing mode.
 * @post The returned value may be passed to @ref strb_setmode.
 */
int strb_getmode(const strb_t *sb );

/**
 * @brief Set editing position.
 *
 * Instead of passing a position parameter to every function, an internal position indicator is stored in the
 * string buffer object. The initial position is the end of the string. It is updated by operations on the string.
 *
 * Repositioning can fail if the requested position is not supported by the library, in which case the
 * current position is unchanged. Any value returned by @ref strb_tell is accepted by @ref strb_seek. 
 *
 * @param[in,out] sb   String buffer.
 * @param         pos  New position, in characters.
 * @return If successful, zero, otherwise EOF.
 * @post Alters the behaviour of @ref strb_putc, @ref strb_unputc, @ref strb_puts, @ref strb_nputs,
 *       @ref strb_vputf, @ref strb_putf, @ref strb_write and @ref strb_delto.
 */
int strb_seek(strb_t *sb, size_t pos);

/**
 * @brief Get editing position.
 *
 * @param[in] sb  String buffer.
 * @return Current editing position.
 * @post The returned value may be passed to @ref strb_seek.
 */
size_t strb_tell(strb_t const *sb);

int strb_putc(strb_t *sb, int c); 
int strb_nputc(strb_t *sb, int c, size_t n);
int strb_unputc(strb_t *sb);
int strb_puts(strb_t *sb, const char *str );
int strb_nputs(strb_t *sb, const char *str, size_t n);

#if !STRB_FREESTANDING
int strb_vputf(strb_t *sb, const char *format, va_list args );
int strb_putf(strb_t *sb, const char *format, ...);
#endif

_Optional char *strb_write(strb_t *sb, size_t n);
void strb_wrote(strb_t *sb);

void strb_delto(strb_t *sb, size_t pos);

int strb_cpy(strb_t *sb, const char *str );
int strb_ncpy(strb_t *sb, const char *str, size_t n);

#if !STRB_FREESTANDING
int strb_vprintf(strb_t *sb, const char *format, va_list args );
int strb_printf(strb_t *sb, const char *format, ...);
#endif

bool strb_error(strb_t const *sb );
void strb_clearerr(strb_t *sb);
