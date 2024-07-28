# strb_t
A new type and functions to manage strings, as proposed in WG14 papers N3250, N3296 and N3306. Its goal is to eliminate a source of many common programmer errors. The new interface is designed to be as familiar and ergonomic as possible.

See https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3296.pdf

The prototype can be configured with -DSTRB_STATIC_ALLOC (no dynamic allocation), -DSTRB_FREESTANDING (no static allocation either), and/or -DDEBUGOUT (extra messages to stderr) and -DNDEBUG (no assertions).

I haven't written a full test suite or anything, but it seems pretty solid for the use-cases I've tried so far. It also gives a good idea of the size of the code likely to be required for an implementation, or different subsets of the specified functionality.
