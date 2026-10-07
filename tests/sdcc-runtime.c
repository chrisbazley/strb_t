// Copyright 2026 Christopher Bazley
// SPDX-License-Identifier: MIT

#include <stdio.h>

// uCsim simulator interface, configured by the CI job.
volatile unsigned char __at (0xff00) simif;

int putchar(int c)
{
    simif = 'w';
    simif = (unsigned char)c;
    return (unsigned char)c;
}

int strb_test_main(void);

int main(void)
{
    const int result = strb_test_main();
    puts(result ? "STRB TEST FAIL" : "STRB TEST PASS");
    simif = 's';
    for (;;) {}
}
