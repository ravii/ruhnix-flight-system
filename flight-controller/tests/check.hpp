/** Minimal assertion helpers for host unit tests (no third-party framework needed). */
#pragma once

#include <cmath>
#include <cstdio>

namespace test
{
inline int failures = 0;
}

#define CHECK(cond)                                                                       \
    do                                                                                    \
    {                                                                                     \
        if (!(cond))                                                                      \
        {                                                                                 \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++test::failures;                                                             \
        }                                                                                 \
    } while (0)

#define CHECK_NEAR(a, b, tol) CHECK(std::fabs((a) - (b)) < (tol))

#define TEST_MAIN_RESULT(name)                                                            \
    (test::failures ? (std::fprintf(stderr, "%d check(s) failed\n", test::failures), 1) : \
                      (std::printf("all %s tests passed\n", name), 0))
