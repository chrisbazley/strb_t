// Copyright 2024 Christopher Bazley
// SPDX-License-Identifier: MIT
/**
 * @file strb.h
 * @author Christopher Bazley (chris.bazley@arm.com)
 * @version 0.1
 * @date 2024-07-26
 *
 * @copyright Copyright (c) 2024
 *
 */
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

/**
 * Whether the interface has user-allocated string buffer state objects.
 */
#define STRB_EXT_STATE 1

/**
 * Whether the interface provides the @ref strb_unputc function.
 */
#define STRB_UNPUTC 0

/**
 * Whether the interface provides the @ref strb_restore function.
 */
#define STRB_RESTORE 0

#if STRB_FREESTANDING
// No static or dynamic allocation
/**
 * Maximum number of allocated string buffer objects.
 */
#define STRB_MAX SIZE_MAX
/**
 * Type capable of representing all supported character positions and buffer sizes.
 */
typedef uint8_t strbsize_t;
/**
 * Macro to be used to print values of type @ref strbsize_t
 */
#define PRIstrbsize PRIu8

/**
 * Maximum string size, in characters (including null terminator).
 */
#define STRB_MAX_SIZE UINT8_MAX

/**
 * Macro used to suppress variably modified types in parameter lists.
 */
#define STRB_SIZE_HINT(X)

#elif STRB_STATIC_ALLOC
// About 2KB of static storage
/**
 * Maximum number of allocated string buffer objects.
 */
#define STRB_MAX (8) // must not exceed 8

/**
 * Type capable of representing all supported character positions and buffer sizes.
 */
typedef uint8_t strbsize_t;

/**
 * Macro to be used to print values of type @ref strbsize_t
 */
#define PRIstrbsize PRIu8

/**
 * Maximum string size, in characters (including null terminator).
 */
#define STRB_MAX_SIZE (256-8)

/**
 * Macro used to suppress variably modified types in parameter lists.
 */
#define STRB_SIZE_HINT(X)

#else
// Unlimited dynamic allocation
/**
 * Maximum number of allocated string buffer objects.
 */
#define STRB_MAX SIZE_MAX

/**
 * Type capable of representing all supported character positions and buffer sizes.
 */
typedef uint16_t strbsize_t;

/**
 * Macro to be used to print values of type @ref strbsize_t
 */
#define PRIstrbsize PRIu16

/**
 * Maximum string size, in characters (including null terminator).
 */
#define STRB_MAX_SIZE UINT16_MAX

/**
 * Buffer size, in characters, substituted by @ref strb_alloc when the requested size is too big or small.
 */
#define STRB_DFL_SIZE (256)

/**
 * Maximum buffer size, in characters, allocated as part of a @ref strb_t object rather than separately.
 */
#define STRB_MAX_INTERNAL_SIZE (256)

/**
 * Macro used to suppress variably modified types in parameter lists.
 */
#define STRB_SIZE_HINT(X) (X)

/**
 * Factor by which to grow the string buffer size when space is exhausted.
 */
#define STRB_GROW_FACTOR (2)

#endif

/**
 * Qualifier indicating optional objects
 *
 * A type qualifier named _Optional has been used throughout this interface to clarify declarations.
 * This qualifier was proposed by N3089, which was reviewed by the committee at the Strasbourg meeting
 * in January 2024 with strong consensus to proceed. It can be ignored (here, by defining it as an
 * empty macro) without substantially changing the meaning of the code.
 */
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
#if STRB_UNPUTC
    char unputc_char;
#endif
#if STRB_RESTORE
    char restore_char;
#endif
    char flags;
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
 * must exceed the program's usage of any strb_t pointers derived from it. The effect on any derived
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
 * The initial string length is 0. This function cannot fail.
 *
 * The effects of strb_... functions on an external array are always immediately visible
 * and the string therein is always null terminated. Operations on the string buffer
 * can use the whole of the external array but never allocate any extra storage.
 *
 * @param[out] sbs   String buffer state.
 * @param      size  Size of the array to be used instead of an internal buffer (must not be 0).
 * @param[out] buf   The array to be used instead of an internal buffer.
 *
 * @return Address of the created string buffer object.
 * @post The user may call @ref strb_free to free the string buffer object, but
 *       it is not required and has no effect.
 * @post The created string buffer object becomes invalid if the storage for @p sbs or @p buf
 *       is deallocated.
 * @post The created string buffer object becomes invalid if the @p sbs object is
 *       modified (other than as a side-effect of calling a strb_... function which is not
 *       @ref strb_reuse or @ref strb_use).
 * @post @ref strb_tell and @ref strb_len return 0.
 * @post A call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post A call to @ref strb_unputc will fail until a character has been put
 *       into the buffer.
 * @post A call to @ref strb_error will return false until an error occurs.
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
 * and the string therein is always null terminated. Operations on the string buffer
 * can use the whole of the external array but never allocate any extra storage.
 *
 * @param[out]    sbs   String buffer state.
 * @param         size  Size of the array to be used instead of an internal buffer (must not be 0).
 * @param[in,out] buf   The array to be used instead of an internal buffer.
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user may call @ref strb_free to free the string buffer object, but
 *       it is not required and has no effect.
 * @post The created string buffer object becomes invalid if the storage for @p sbs or @p buf
 *       is deallocated.
 * @post The created string buffer object becomes invalid if the @p sbs object is
 *       modified (other than as a side-effect of calling a strb_... function which is not
 *       @ref strb_reuse or @ref strb_use).
 * @post If successful, @ref strb_tell and @ref strb_len return the reused string length.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post If successful, the last character of the reused string (if any) can be
 *       removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_error will return false until an error occurs.
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
 * and the string therein is always null terminated. Operations on the string buffer
 * can use the whole of the external array but never allocate any extra storage.
 *
 * @param      size  Size of the array to be used instead of an internal buffer (must not be 0).
 * @param[out] buf   The array to be used instead of an internal buffer.
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object. 
 * @post If successful, @ref strb_tell and @ref strb_len return 0.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post If successful, a call to @ref strb_unputc will fail until a character has been put
 *       into the buffer.
 * @post If successful, a call to @ref strb_error will return false until an error occurs.
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
 * and the string therein is always null terminated. Operations on the string buffer
 * can use the whole of the external array but never allocate any extra storage.
 *
 * @param         size  Size of the array to be used instead of an internal buffer (must not be 0).
 * @param[in,out] buf   The array to be used instead of an internal buffer.
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object.
 * @post If successful, @ref strb_tell and @ref strb_len return the reused string length.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post If successful, the last character of the reused string (if any) can be
 *       removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_error will return false until an error occurs.
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
 * The initial string length is 0. This function does not necessarily allocate
 * storage for a string immediately, providing that all other functions operate as
 * if it had been allocated. Subsequent operations on the string buffer may allocate or
 * free extra storage, if supported by the implementation.
 *
 * @param size  A hint about how much storage to allocate for an internal buffer
 *              (where 0 means default size).
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object. 
 * @post If successful, @ref strb_tell and @ref strb_len return 0.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post If successful, a call to @ref strb_error will return false until an error occurs.
 */
_Optional strb_t *strb_alloc(size_t size);

/**
 * @brief Create a string buffer object with internal storage by duplicating a string
 *
 * Allocates storage for a string buffer object, initialises it, and returns its address.
 * If storage allocation fails, a null pointer is returned.
 *
 * Copies the given string into internal storage, including its null terminator.
 * Subsequent operations on the string buffer may automatically allocate or free extra
 * storage, if supported by the implementation.
 *
 * @param[in] str  A string to be copied as the initial content of the buffer.
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object. 
 * @post If successful, @ref strb_tell and @ref strb_len return the number of characters copied.
 * @post If successful, the last character copied (if any) can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post If successful, a call to @ref strb_error will return false until an error occurs.
 */
_Optional strb_t *strb_dup(const char *str);

/**
 * @brief Create a string buffer object with internal storage by duplicating a sequence of characters
 *
 * Allocates storage for a string buffer object, initialises it, and returns its address.
 * If storage allocation fails, a null pointer is returned.
 *
 * Copies up to @p n characters from the array designated by @p str into internal storage, then
 * appends a null terminator. A null character and any characters following it are not copied.
 * Subsequent operations on the string buffer may automatically allocate or free extra
 * storage, if supported by the implementation.
 *
 * @param[in] str   An array of characters to be copied as the initial content of the buffer.
 * @param     n     Maximum number of characters to copy from @p str.
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object. 
 * @post If successful, @ref strb_tell and @ref strb_len return the number of characters copied.
 * @post If successful, the last character copied (if any) can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post If successful, a call to @ref strb_error will return false until an error occurs.
 */
_Optional strb_t *strb_ndup(const char *str, size_t n);

/**
 * @brief Create a string buffer object with internal storage by parsing a format string.
 *
 * Allocates storage for a string buffer object, initialises it, and returns its address.
 * If storage allocation fails, a null pointer is returned.
 *
 * The @p format parameter specifies how to convert subsequent arguments to generate a string,
 * which forms the initial content of the buffer. Subsequent operations on the buffer may
 * automatically allocate or free extra storage, if supported by the implementation.
 *
 * @param[in] format   Specifies how to convert subsequent arguments to generate a string.
 * @param     ...      Arguments to be substituted into the generated string.
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object. 
 * @post If successful, @ref strb_tell and @ref strb_len return the number of characters generated.
 * @post If successful, the last character generated (if any) can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post If successful, a call to @ref strb_error will return false until an error occurs.
 */
_Optional strb_t *strb_aprintf(const char *format, ...);

/**
 * @brief Create a string buffer object with internal storage by parsing a format string.
 *
 * Allocates storage for a string buffer object, initialises it, and returns its address.
 * If storage allocation fails, a null pointer is returned.
 *
 * The @p format parameter specifies how to convert subsequent arguments to generate a string,
 * which forms the initial content of the buffer. Subsequent operations on the buffer may
 * automatically allocate or free extra storage, if supported by the implementation.
 *
 * @param[in] format   Specifies how to convert subsequent arguments to generate a string.
 * @param     args     Variable argument list to be substituted into the generated string.
 *
 * @return Address of the created string buffer object, or a null pointer on failure.
 * @post The user is responsible for calling @ref strb_free to free the string buffer object.
 * @post If successful, @ref strb_tell and @ref strb_len return the number of characters generated.
 * @post If successful, the last character generated (if any) can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post If successful, a call to @ref strb_error will return false until an error occurs.
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
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
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
 * @return Address of the character stored at position 0 in the string buffer.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post The returned pointer is valid until the next call to a strb_... function.
 */
const char *strb_ptr(strb_t const *sb);

/**
 * @brief Get the number of characters stored in a string buffer.
 *
 * A null character may appear anywhere in string buffer, since it is easy to insert
 * one by calling @c strb_putf(sb, "%c", 0). It would be hard to prevent such misuse.
 * Consequently, @c strb_len(sb) and @c strlen(strb_ptr(sb)) can return different results.
 * The returned length is never less than, but may be greater than, the true string length.
 *
 * @param[in] sb  String buffer.
 * @return Number of characters stored in the string buffer.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 */
size_t strb_len(strb_t const *sb);

/**
 * @brief Editing mode.
 */
enum {
  /**
   * Insertion moves characters upward and lengthens the string.
   * Deletion in insertion mode moves characters downward and shortens the string.
   */
  strb_insert,
  /**
   * Overwrite characters at the current position and lengthen the string only as necessary (at the end).
   * Deletion in overwrite mode merely repositions the next operation.
   */
  strb_overwrite
};

/**
 * @brief Set the editing mode of a string buffer.
 *
 * Instead of providing variants of every function to insert or overwrite characters, this behaviour is
 * controlled by a mode stored in the string buffer object. The initial mode is @ref strb_insert.
 *
 * Changing the mode can fail if the requested mode is not supported by the library, in which case the
 * current mode is unchanged. Any value returned by @ref strb_getmode is accepted by @ref strb_setmode.
 *
 * @param[in,out] sb  String buffer.
 * @param         mode New mode.
 * @return Zero if successful, otherwise @c EOF.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post Alters the behaviour of @ref strb_putc, @ref strb_unputc, @ref strb_puts, @ref strb_nputs,
 *       @ref strb_vputf, @ref strb_putf, @ref strb_write and @ref strb_delto.
 * @post If successful, a call to @ref strb_unputc will fail until a character has been put
 *       into the buffer.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_setmode(strb_t *sb, int mode);

/**
 * @brief Get the editing mode of a string buffer.
 *
 * @param[in] sb  String buffer.
 * @return Current editing mode.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post The returned value may be passed to @ref strb_setmode.
 */
int strb_getmode(const strb_t *sb );

/**
 * @brief Set the editing position of a string buffer.
 *
 * Instead of passing a position parameter to every function, an internal position indicator is stored in the
 * string buffer object. The initial position is the end of the string. It is updated by operations on the string.
 *
 * Repositioning can fail if the requested position is not supported by the library, in which case the
 * current position is unchanged. Any value returned by @ref strb_tell is accepted by @ref strb_seek.
 *
 * Passing a position greater than the string buffer length is allowed and does not change the
 * length. If characters are later written beyond the end of the string, then the length will be updated.
 * The initial value of any intervening characters will be 0.
 *
 * @param[in,out] sb   String buffer.
 * @param         pos  New position, in characters.
 * @return Zero if successful, otherwise EOF.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post Alters the behaviour of @ref strb_putc, @ref strb_unputc, @ref strb_puts, @ref strb_nputs,
 *       @ref strb_vputf, @ref strb_putf, @ref strb_write and @ref strb_delto.
 * @post A call to @ref strb_unputc will fail until a character has been put into the buffer.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_seek(strb_t *sb, size_t pos);

/**
 * @brief Get the editing position of a string buffer.
 *
 * @param[in] sb  String buffer.
 * @return Current editing position.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post The returned value may be passed to @ref strb_seek.
 */
size_t strb_tell(strb_t const *sb);

/**
 * @brief Put a character into a string buffer.
 *
 * Copies one character into the buffer at the current position and increments
 * the position indicator. If the mode is @ref strb_insert, then characters at
 * the current position are first moved upward to make space; otherwise, no
 * characters are moved. Additional storage is allocated if permitted and necessary.
 *
 * @param[in,out] sb  String buffer.
 * @param         c   Character to put.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @return If successful, the character written, otherwise EOF.
 * @post If successful, the position indicator was incremented and the string length
*        may have increased by one (depending on editing position and mode).
 * @post If successful, the character written can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_putc(strb_t *sb, int c);

/**
 * @brief Put a character into a string buffer multiple times.
 *
 * Equivalent to calling @ref strb_putc @p n times with the given value of @p c.
 *
 * @param[in,out] sb  String buffer.
 * @param         c   Character to put.
 * @param         n   The number of times to copy the specified character into the buffer. 
 * @return If successful, the character written, otherwise EOF.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post If successful, the position indicator has advanced by @p n characters and the string
*        length has increased by not more than @p n (depending on editing position and mode).
 * @post If successful, the last character written can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_nputc(strb_t *sb, int c, size_t n);

#if STRB_UNPUTC
/**
 * @brief Undo the last put character operation.
 *
 * Restores one character behind the current position and decrements the position indicator. 
 * If the mode is @ref strb_insert, then any characters in the buffer at the former
 * position are moved downward to the new position; otherwise, no characters are moved. This
 * function may substitute a smaller buffer at the implementer's discretion.
 *
 * A call to a function that deletes characters or sets the position/mode will discard any restorable
 * characters in the string buffer object. Only one character is guaranteed to be restorable, regardless of
 * mode. If this function is called too many times without an intervening write, then it may fail.
 *
 * @param[in,out] sb  String buffer.
 * @return If successful, the character removed, otherwise EOF.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post If successful, the restored character cannot be restored again.
 * @post If successful, the position indicator was decremented.
 * @post If successful in @ref strb_insert mode, the string length was decremented.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_unputc(strb_t *sb);
#endif

/**
 * @brief Put a string into a string buffer.
 *
 * Copies a string into the buffer at the current position as if by calling
 * @ref strb_putc for each character except for the terminating null, which is not
 * copied.
 *
 * @param[in,out] sb   String buffer.
 * @param[in]     str  A string to be copied into the buffer.
 * @return Zero if successful, otherwise EOF.
 *         (This is stricter than @c fputs, which returns only 'a nonnegative value' if successful.)
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post If successful, the position indicator has advanced by the length of the given @p str
 *       and the string length has increased by not more than the length of the given @p str.
 * @post If successful, the last character copied can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_puts(strb_t *sb, const char *str);

/**
 * @brief Put a sequence of characters into a string buffer.
 *
 * Copies up to @p n characters from the array designated by @p str into the buffer at the current position
 * as if by calling @ref strb_putc for each character. A null character and any characters
 * following it are not copied.
 *
 * @param[in,out] sb   String buffer.
 * @param[in]     str  A string to be copied into the buffer.
 * @param         n     Maximum number of characters to copy from @p str.
 * @return Zero if successful, otherwise EOF.
 *         (This is stricter than @c fputs, which returns only 'a nonnegative value' if successful.)
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post If successful, the position indicator has advanced by the number of characters copied
 *       and the string length has increased by not more than the number of characters copied.
 * @post If successful, the last character copied can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_nputs(strb_t *sb, const char *str, size_t n);

#if !STRB_FREESTANDING
/**
 * @brief Put a generated string into a string buffer.
 *
 * Generates characters under control of a format string, which are written into the buffer at the
 * current position as if by calling @ref strb_putc for each character.
 *
 * @param[in,out] sb       String buffer.
 * @param[in]     format   Specifies how to convert subsequent arguments to generate a string.
 * @param         args     Variable argument list to be substituted into the generated string.
 * @return Zero if successful, otherwise EOF.
 *         (This differs from @c fprintf, which returns 'the number of characters transmitted, or a negative value'.)
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post If successful, the position indicator has advanced by the number of characters generated 
 *       and the string length has increased by not more than the number of characters generated.
 * @post If successful, the last character written can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_vputf(strb_t *sb, const char *format, va_list args);

/**
 * @brief Put a generated string into a string buffer.
 *
 * Generates characters under control of a format string, which are written into the buffer at the
 * current position as if by calling @ref strb_putc for each character.
 *
 * @param[in,out] sb       String buffer.
 * @param[in]     format   Specifies how to convert subsequent arguments to generate a string.
 * @param         ...      Arguments to be substituted into the generated string.
 * @return Zero if successful, otherwise EOF.
 *         (This differs from @c fprintf, which returns 'the number of characters transmitted, or a negative value'.)
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post If successful, the position indicator has advanced by the number of characters generated
 *       and the string length has increased by not more than the number of characters generated.
 * @post If successful, the last character written can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_putf(strb_t *sb, const char *format, ...);
#endif

/**
 * @brief Prepare to write characters directly into a string buffer.
 *
 * Allows direct insertion of strings into the buffer, particularly by third-party
 * functions which cannot be modified to operate on a string buffer object.
 * Because it returns an unqualified pointer, this function also provides a mechanism
 * for avoiding casts when calling functions that do not accept the address of a @c const @c char.
 *
 * The @p n parameter indicates the number of characters expected to be written into the
 * buffer (not including any null terminator). If the mode is @ref strb_insert, then @ref strb_write
 * will move characters at the current position upward to make space; otherwise, no characters are
 * moved. Additional storage is allocated if necessary to allow @p n + 1 characters to be written.
 *
 * @param[in,out] sb  String buffer.
 * @param         n   The number of characters expected to be written into the buffer.
 * @return A pointer to the position where the first character should be written, or a
 *         null pointer on failure.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post The existing contents of the buffer are unmodified.
 *       The initial value of any characters allocated beyond the previous end of the buffer is 0.
 * @post If successful, the position indicator has advanced by @p n characters
 *       and the string length has increased by not more than @p n characters.
 * @post After copying up to @p n + 1 characters (including any null terminator) into the buffer at 
 *       the returned address, the user may call @ref strb_restore to restore the character that was
 *       at offset @p n from the current position before the call to @ref strb_write. 
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
_Optional char *strb_write(strb_t *sb, size_t n);

/**
 * @brief Restore a character that may have been overwritten by a preceding write.
 *
 * Restores the character previously at the current position before the most recent
 * call to @ref strb_write.
 *
 * This function exists to make it efficient and simple to write strings (as opposed to
 * unterminated character sequences) directly into a string buffer. Since strings are
 * terminated by a null character, any character overwritten by this terminator must be
 * restored to prevent unintentional truncation.
 *
 * For example, prepending "fish" to "cat" should not result in the string "fish\0at".
 * A call to @ref strb_restore would restore the overwritten character 'c' in-place,
 * regardless of the current editing mode.
 *
 * This is likely to be more efficient than reserving space for one extra character
 * when calling @ref strb_write (to produce "fish\0cat") and then deleting the excess
 * null character by calling @ref strb_unputc (which might need to move characters).
 *
 * @param[in,out] sb  String buffer.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post If there was no intervening call that put, delete, or restore characters, or
 *       that set the position, then the character at the current position is restored
 *       the value that it had prior to the most recent call to @ref strb_write.
 */
void strb_restore(strb_t *sb);

/**
 * @deprecated Original name of strb_restore.
 */
#define strb_wrote(sb) strb_restore(sb)

/**
 * @brief Delete characters to a specified position in a string buffer.
 *
 * Deletes characters between the current position and a position specified by the caller.
 * If the mode is @ref strb_insert, then the character at the higher of the two positions is
 * moved to the lower position, and any following characters are moved the same distance; otherwise,
 * no characters are moved. Afterwards, the value of the position indicator is the lower of the two
 * positions. This function may substitute a smaller buffer at the implementer's discretion.
 *
 * Passing a position greater than the string length is allowed: @c SIZE_MAX or @c (size_t)-1
 * can be used as a shorthand to delete all characters between the current position and the end
 * of the string.
 *
 * @param[in,out] sb  String buffer.
 * @param         pos New position, in characters.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post The position indicator is at the lower of @p pos and the previous position and
 *       the string length has decreased by not more than the difference between the two.
 * @post A call to @ref strb_restore will have no effect until @ref strb_write has been called.
 */
void strb_delto(strb_t *sb, size_t pos);

/**
 * @brief Copy a string into a string buffer.
 *
 * Replaces the string in a buffer by copying a string.
 *
 * @param[in,out] sb  String buffer.
 * @param[in]     str  A string to be copied as the new content of the buffer.
 * @return Zero if successful, otherwise EOF.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post If successful, @ref strb_tell and @ref strb_len return the number of characters copied.
 * @post If successful, the last character copied (if any) can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_cpy(strb_t *sb, const char *str);

/**
 * @brief Copy a sequence of characters into a string buffer.
 *
 * Replaces the string in a buffer by copying up to @p n characters from the array
 * designated by @p str, then appends a null terminator. A null character and any characters
 * following it are not copied.
 *
 * @param[in,out] sb   String buffer.
 * @param[in]     str  An array of characters to be copied as the new content of the buffer.
 * @param         n    Maximum number of characters to copy from @p str.
 * @return Zero if successful, otherwise EOF.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post If successful, @ref strb_tell and @ref strb_len return the number of characters copied.
 * @post If successful, the last character copied (if any) can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_ncpy(strb_t *sb, const char *str, size_t n);

#if !STRB_FREESTANDING

/**
 * @brief Print a generated string into a string buffer.
 *
 * Replaces the string in a buffer by generating characters under control of a format string.
 *
 * @param[in,out] sb       String buffer.
 * @param[in]     format   Specifies how to convert subsequent arguments to generate a string.
 * @param         args     Variable argument list to be substituted into the generated string.
 * @return Zero if successful, otherwise EOF.
 *         (This differs from @c vsprintf, which returns 'the number of characters written in the array,
 *         not counting the terminating null character, or a negative value'.)
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post If successful, @ref strb_tell and @ref strb_len return the number of characters generated.
 * @post If successful, the last character written can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_vprintf(strb_t *sb, const char *format, va_list args);

/**
 * @brief Print a generated string into a string buffer.
 *
 * Replaces the string in a buffer by generating characters under control of a format string.
 *
 * @param[in,out] sb       String buffer.
 * @param[in]     format   Specifies how to convert subsequent arguments to generate a string.
 * @param         ...      Arguments to be substituted into the generated string.
 * @return Zero if successful, otherwise EOF.
 *         (This differs from @c sprintf, which returns 'the number of characters written in the array,
 *         not counting the terminating null character, or a negative value'.)
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post If successful, @ref strb_tell and @ref strb_len return the number of characters generated.
 * @post If successful, the last character written can be removed by @ref strb_unputc.
 * @post If successful, a call to @ref strb_restore will have no effect until
 *       @ref strb_write has been called.
 * @post On failure, a call to @ref strb_error will return true until
 *       @ref strb_clearerr has been called.
 */
int strb_printf(strb_t *sb, const char *format, ...);
#endif

/**
 * @brief Get the error indicator of a string buffer.
 *
 * Additional storage for the underlying character array may be allocated automatically as the string
 * grows, for example because of calls to @ref strb_puts. When using a character array passed to @ref strb_use
 * or @ref strb_reuse, or an internal buffer of fixed size, any attempt to allocate more storage fails.
 * If any attempt at storage allocation fails, the state of the buffer, mode, and position indicator are as
 * if the failed operation had never been attempted. This allows use in interactive software running on
 * machines with limited physical memory and no swap file.
 *
 * An error indicator is stored to allow deferred error handling. Its value can be read at any time by
 * calling @ref strb_error. An error can only be cleared explicitly (by calling @ref strb_clearerr).
 * The error indicator is set as a side-effect of any function that returns an error value, including
 * because of attempts to reposition to an unsupported index or select an unsupported editing mode.
 * Consequently, it is a reliable indication of whether the current string is valid.
 *
 * @param[in] sb  String buffer.
 * @return True if an error occurred since the most recent call to @ref strb_clearerr, otherwise false.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 */
bool strb_error(strb_t const *sb);

/**
 * @brief Clear the error indicator of a string buffer.
 *
 * @param[in,out] sb  String buffer.
 * @pre  The given @p sb address was returned by @ref strb_use, @ref strb_reuse,
 *       @ref strb_alloc, @ref strb_dup, @ref strb_ndup, @ref strb_aprintf or @ref strb_vaprintf.
 * @post A call to @ref strb_error will return false until an error occurs.
 */
void strb_clearerr(strb_t *sb);
