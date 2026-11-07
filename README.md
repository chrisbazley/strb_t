# strb_t
A new type and functions to manage strings, as proposed in [WG14](https://www.open-std.org/jtc1/sc22/wg14/) papers N3250, N3296 and N3306. Its goal is to eliminate a source of many common programmer errors. The new interface is designed to be as familiar and ergonomic as possible.

See the [slides](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3276.pdf) presented
to WG14 and the [latest paper](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3306.pdf)
submitted to WG14.

The common sources are split so that linking a static archive pulls in only the
required groups of operations:

| Source | Responsibility |
| --- | --- |
| `strb_core.c` | Buffer access, length and private initialisation helpers |
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
`STRB_UNPUTC=0`, and `strb_external.c` when `STRB_EXT_STATE=0`, because those
units then have no enabled function definitions. Editing mode and current
position each have their setter and getter in the same translation unit. Error
indicator operations are grouped together. The core retains buffer access,
length and private initialisation helpers.

Additional functions are grouped by responsibility. `strb_constructors.c`
provides shared allocating construction and duplication functions;
`strb_format.c` provides formatted construction and output. Their availability
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
- **Optional features:** define `STRB_UNPUTC`, `STRB_RESTORE`, `STRB_EXT_STATE`
  and `STRB_REUSE_CONST` consistently for every library and client translation
  unit. Omitting a source file alone does not disable a feature: for example,
  `STRB_UNPUTC=0` also removes saved-character state and its maintenance in other
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

The [freestanding implementation](https://godbolt.org/z/vYWa1PrTc) produces 1,725 bytes of instructions for the 65C02 with SDCC trunk, or a 636-byte `.text` section for Cortex-M0 with ARM GCC trunk (measured on 6 November 2026). Both configurations disable assertions, `STRB_UNPUTC` and `STRB_REUSE_CONST`; GCC uses `-Oz -mcpu=cortex-m0 -mthumb -mfloat-abi=soft`. The Arm figure includes literal pools and alignment padding. These figures exclude RAM storage, debug information and the external `memcpy`, `memmove` and `memset` implementations. GCC also calls an external `strnlen`; the SDCC figure includes its 59-byte local fallback.

## Configuration tests

I haven't written a full test suite or anything, but it seems pretty solid for the use-cases I've tried so far. It also gives a good idea of the size of the code likely to be required for an implementation, or different subsets of the specified functionality.

CMake builds and runs the existing suite with each optional interface disabled:
`STRB_EXT_STATE`, `STRB_UNPUTC`, `STRB_RESTORE` and `STRB_REUSE_CONST`. It also
tests a minimal configuration with all four disabled. These variants cover
dynamic allocation, the static pool, and freestanding storage where applicable.
Freestanding configurations without external state are omitted because they
provide no constructors and would run no string-buffer tests.

The feature-off targets use their own interface definitions; the design examples
continue to use the full-interface `strb` target. Definitions propagate from each
library target to its consumers. The additional test-only archives are not
installed. Run all 17 registered tests with:

```
cmake -S . -B build -DWARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Documentation

[Documentation](https://chrisbazley.github.io/strb_t/strb_8h.html) for the interface is generated using Doxygen.

[Interface design and usage](https://chrisbazley.github.io/strb_t/interface_design.html) explains the positional output model, why there are no separate append/prepend/insert functions, and how to use the same text producer in different editing contexts.


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

## FAQ

Q1: Won't people confuse it with the ARM instruction [STRB](https://developer.arm.com/documentation/ddi0602/2025-12/Base-Instructions/STRB--immediate---Store-register-byte--immediate--)?

A1: Calling it strb was also an in-joke, because that's what it does.

Q2: Could it be a library or does it have to be part of the language?

A2: Incorporating safe string functions into the standard would:
- Standardise existing best practice.
- Reduce the level of experience needed to write correct programs.
- Provide a better precedent to follow when users design their own interfaces.
- Make it easier to write interoperable user-designed libraries.
- Ensure that the implementation is optimised for the target platform.
