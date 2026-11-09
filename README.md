# strb_t

## Overview

A new type and functions to manage strings, as proposed in [WG14](https://www.open-std.org/jtc1/sc22/wg14/) papers N3250, N3296 and N3306. Its goal is to eliminate a source of many common programmer errors. The new interface is designed to be as familiar and ergonomic as possible.

See the [slides](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3276.pdf) presented
to WG14 and the [latest paper](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3306.pdf)
submitted to WG14.

## Configuration

The common sources are split so that linking a static archive pulls in only the
required groups of operations:

| Source | Responsibility |
| --- | --- |
| `strb_core.c` | Buffer access, length, emptying and private initialisation helpers |
| `strb_external.c` | Construction with caller-supplied state and character storage |
| `strb_strnlen.c` | Private bounded-length fallback, where the platform needs it |
| `strb_mode.c` | Setting and getting the editing mode |
| `strb_pos.c` | Setting and getting the current position |
| `strb_error.c` | Setting, querying and clearing the error indicator |
| `strb_write.c` | Write preparation, storage growth, tail movement and boundary restoration |
| `strb_split.c` | Temporary string splitting |
| `strb_delto.c` | Deleting between two positions |
| `strb_unputc.c` | Undoing the last character write |
| `strb_puts.c` | String copying and output |
| `strb_putc.c` | Single and repeated character output |

These are the shared operation sources. CMake omits `strb_unputc.c` when
`STRB_HAS_UNPUTC=0`, because that unit then has no enabled function definitions.
Editing mode and current
position each have their setter and getter in the same translation unit. Error
indicator operations are grouped together. The core retains buffer access,
length and private initialisation helpers.

Additional functions are grouped by responsibility. `strb_constructors.c`
provides duplication functions; `strb_format.c` provides formatted construction
and output. Their availability
in this prototype is a configuration choice, not a restriction on what
freestanding implementations may provide.

Each configuration selects one implementation of storage allocation, growth and
release:

| Configuration | Additional sources | Definition |
| --- | --- | --- |
| Dynamic allocation | `strb_constructors.c`, `strb_format.c`, `strb_dynamic.c` | None |
| Static pool | `strb_constructors.c`, `strb_format.c`, `strb_static.c` | `STRB_STATIC_ALLOC` |
| Freestanding | None | `STRB_FREESTANDING` |

Storage selection and optional features serve different purposes:

- **Storage backend:** choose the dynamic, static-pool or freestanding build.
  Its source list and matching storage macro must agree, because the macro also
  controls the public interface and shared state layout. CMake targets and Make
  recipes select this combination together; the two allocation backends are
  alternative implementations and must not be linked together.
- **Optional features:** define `STRB_HAS_UNPUTC` and `STRB_HAS_RESTORE` consistently
  for every library and client translation unit. Omitting a source file alone
  does not disable a feature: for example,
  `STRB_HAS_UNPUTC=0` also removes saved-character state and its maintenance in other
  functions. The CMake feature-off targets derive any source omissions from
  these definitions. Feature-specific and storage-specific translation units
  use `#error` to reject incompatible macro settings.
- **Unused operations:** link the static archive normally. Its separate objects
  let the linker leave unused groups out of the executable without changing the
  configured interface. No manual pruning of operation sources is required.

The private header `strb_internal.h` shares implementation state; users include
only `strb.h`. Private functions with external linkage use the `strb__` prefix;
public functions use a single underscore after `strb`. `DEBUGOUT` enables
messages to stderr, and `NDEBUG` disables assertions.

The [freestanding implementation example](https://godbolt.org/z/6PcvqWvrx)
includes all applicable operation sources at commit
[`848cb11`](https://github.com/chrisbazley/strb_t/commit/848cb11b419c73016fe63f199b7b840ea2ba0220).
Measured on 9 November 2026, it produces 1,918 bytes in the `CODE` area for the
65C02 with SDCC 4.6.4 r17150, or a 730-byte `.text` section for Cortex-M0 with
ARM GCC trunk (17.0.0, 20261108). These sizes were obtained by assembling the
compiler output. The example compiles the sources together as one translation
unit; linking a static archive can include fewer functions.

Both configurations disable assertions and `STRB_HAS_UNPUTC`; GCC uses
`-Oz -mcpu=cortex-m0 -mthumb -mfloat-abi=soft`. The Arm figure includes
literal pools and alignment padding. Both figures exclude RAM storage, debug
information and the external `memcpy`, `memmove` and `memset` implementations.
GCC also calls an external `strnlen`; the SDCC figure includes its 59-byte local
fallback. In the saved SDCC pane, **Unused labels** and **Library functions** are
unchecked so that function labels and the downloaded implementation remain
visible. **Library functions** is also unchecked for ARM GCC.

## Building

If you have CMake, a build system generator, then you can use it to
build the library and its tests with minimal manual intervention.

For example, use the following commands to build and test on Linux:
```
  cmake -G 'Unix Makefiles' -S . -B build
  cd build
  make
  ctest
```

## Testing

I haven't written a full test suite or anything, but it seems pretty solid for the use-cases I've tried so far. It also gives a good idea of the size of the code likely to be required for an implementation, or different subsets of the specified functionality.

CMake builds and runs the existing suite with each optional interface disabled:
`STRB_HAS_UNPUTC` and `STRB_HAS_RESTORE`. It also
tests a minimal configuration with both disabled. These variants cover
dynamic allocation, the static pool, and freestanding storage where applicable.
Caller-allocated buffer state is always supported, so all three storage configurations
can run the minimal-feature tests.

The feature-off targets use their own interface definitions; the design examples
continue to use the full-interface `strb` target. Definitions propagate from each
library target to its consumers. The additional test-only archives are not
installed. Run all 13 registered tests with:

```
cmake -S . -B build -DWARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Configure with `-DUSE_FORTIFY=ON` to check allocations with Fortify and run
allocation-failure tests in the existing test suite.

## Documentation

[Documentation](https://chrisbazley.github.io/strb_t/strb_8h.html) for the interface is generated using Doxygen.

[Interface design and usage](https://chrisbazley.github.io/strb_t/interface_design.html) explains the positional output model, why there are no separate append/prepend/insert functions, and how to use the same text producer in different editing contexts.


## FAQ

1. **Why does `strb_t` store a current editing position?**

   It lets a text-producing function write without deciding where its output
   belongs. The caller can use the same producer to append, prepend, insert or
   overwrite by choosing the position and mode. This abstracts those decisions
   instead of requiring a different producer for each operation.
   See [N3306, “Positioning” and “Editing”](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3306.pdf#page=43).

2. **Why does `strb_t` have an editing mode?**

   A stored mode avoids providing insertion and overwrite variants of every
   output function. Insertion makes room for new characters; overwriting replaces
   existing characters without shifting the following text. The default is
   insertion, starting at the end of the string, so successive output calls
   naturally append.
   See [N3306, “Insertion and overwriting”](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3306.pdf#page=43).

3. **Why not use slices instead?**

   This interface does not propose a replacement for C's string representation.
   Its purpose is to let functions produce ordinary C strings without committing
   to a particular allocation strategy, editing operation or output position.
   Changing how substrings are represented is a separate issue.
   See [the author's reply in the original proposal discussion](https://www.reddit.com/r/C_Programming/comments/1ctoh24/)
   and [N3306, “String representation”](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3306.pdf#page=4).

4. **Why put this interface in the standard library?**

   Standardisation would let independently written libraries agree on a string
   buffer interface, rather than each inventing its own. The proposal also aims
   to standardise existing best practice, reduce the experience needed to write
   correct programs, and provide a better precedent for other interfaces. It does
   not seek to replace every use of character arrays or user-designed libraries.
   See [N3306, “Rationale”](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3306.pdf#page=3).

5. **Does `strb_t` require dynamic memory allocation?**

   No. `strb_use` and `strb_reuse` can manage caller-supplied character storage
   using caller-supplied state. The same output functions also work with buffers
   whose storage the library allocates. A text producer need not know which
   strategy its caller chose. Freestanding implementations need not provide
   dynamic allocation.
   See [N3306, “External buffers” and “Internal buffers”](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3306.pdf#page=42).

6. **Must every output call be checked immediately?**

   No. A stored error indicator permits deferred checking with `strb_error`
   after a sequence of operations. It remains set until explicitly cleared with
   `strb_clearerr`. The buffer remains null-terminated even on failure, but the
   program must still check for errors before relying on the resulting text.
   See [N3306, “Errors”](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3306.pdf#page=46)
   and [“Error handling”](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3306.pdf#page=68).

7. **Does `strb_t` require changes to the C language?**

   No. It is a library interface, with a prototype implementation in this
   repository. The `_Optional` qualifier used in the proposal clarifies pointer
   declarations; it can be defined as an empty macro without substantially
   changing the meaning of the code.
   See [N3306, “Pointer notation”](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3306.pdf#page=4).

8. **Won't people confuse it with the ARM instruction [STRB](https://developer.arm.com/documentation/ddi0602/2025-12/Base-Instructions/STRB--immediate---Store-register-byte--immediate--)?**

   Calling it strb was also an in-joke, because that's what it does.
