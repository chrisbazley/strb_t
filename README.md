# strb_t
A new type and functions to manage strings, as proposed in WG14 papers N3250, N3296 and N3306. Its goal is to eliminate a source of many common programmer errors. The new interface is designed to be as familiar and ergonomic as possible.

See https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3276.pdf (slides) and https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3306.pdf (latest paper).

`strb_core.c` implements the common interface subset: construction
with supplied state and character storage, accessors, editing and error handling.
Every storage configuration compiles this translation unit.

The additional functions are grouped by responsibility. `strb_constructors.c`
provides shared construction and duplication functions; `strb_format.c` provides
formatted construction and output. Their availability in this prototype is a
configuration choice, not a restriction on what freestanding implementations
may provide.

Each configuration selects one implementation of storage allocation, growth and
release:

| Configuration | Sources in addition to `strb_core.c` | Definition |
| --- | --- | --- |
| Dynamic allocation | `strb_constructors.c`, `strb_format.c`, `strb_dynamic.c` | None |
| Static pool | `strb_constructors.c`, `strb_format.c`, `strb_static.c` | `STRB_STATIC_ALLOC` |
| Freestanding | None | `STRB_FREESTANDING` |

Apply the configuration definition to every translation unit, including code
using the interface. CMake and Make select the corresponding sources and
definitions. The private header `strb_internal.h` shares implementation state;
users include only `strb.h`. `DEBUGOUT` enables messages to stderr, and `NDEBUG`
disables assertions.

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
