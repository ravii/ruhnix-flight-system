/** Minimal assertion helpers for host unit tests (no third-party framework needed). */
#pragma once

#include <cmath>
#include <cstdio>

namespace rfs::fc
{
inline int test_failures = 0;
}  // namespace rfs::fc

#define CHECK(cond)                                                                       \
    do                                                                                    \
    {                                                                                     \
        if (!(cond))                                                                      \
        {                                                                                 \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++rfs::fc::test_failures;                                                     \
        }                                                                                 \
    } while (0)

#define CHECK_NEAR(a, b, tol) CHECK(std::fabs((a) - (b)) < (tol))

#define TEST_MAIN_RESULT(name)                                                          \
    (rfs::fc::test_failures ?                                                           \
            (std::fprintf(stderr, "%d check(s) failed\n", rfs::fc::test_failures), 1) : \
            (std::printf("all %s tests passed\n", name), 0))
