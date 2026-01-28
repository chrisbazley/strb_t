# strb_t
A new type and functions to manage strings, as proposed in WG14 papers N3250, N3296 and N3306. Its goal is to eliminate a source of many common programmer errors. The new interface is designed to be as familiar and ergonomic as possible.

See https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3276.pdf (slides) and https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3306.pdf (latest paper).

The prototype can be configured with -DSTRB_STATIC_ALLOC (no dynamic allocation), -DSTRB_FREESTANDING (no static allocation either), and/or -DDEBUGOUT (extra messages to stderr) and -DNDEBUG (no assertions).

I haven't written a full test suite or anything, but it seems pretty solid for the use-cases I've tried so far. It also gives a good idea of the size of the code likely to be required for an implementation, or different subsets of the specified functionality.

## FAQ

Q1: Won't people confuse it with the ARM instruction [STRB](https://developer.arm.com/documentation/ddi0602/2025-12/Base-Instructions/STRB--immediate---Store-register-byte--immediate--)?

A1: Calling it strb was also an in-joke, because that's what it does.

Q2: Could it be a library or does it have to be part of the language?

A2: Incorporating safe string functions into the standard would:
- Standardize existing best practice.
- Reduce the level of experience needed to write correct programs.
- Provide a better precedent to follow when users design their own interfaces.
- Make it easier to write interoperable user-designed libraries.
  
It's also the type of functionality (like malloc) that might be better optimised for different target platforms instead of each user providing their own implementation that might not be suitable. For example, [the 65C02 code](https://godbolt.org/z/4jx9jv8We) is only about one kilobyte in size.
