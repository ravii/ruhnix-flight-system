/** Minimal assertion helpers for simulator tests (no third-party framework needed). */
#pragma once

#include <cmath>
#include <cstdio>

namespace rfs::sim
{
inline int test_failures = 0;
}  // namespace rfs::sim

#define CHECK(cond)                                                                       \
    do                                                                                    \
    {                                                                                     \
        if (!(cond))                                                                      \
        {                                                                                 \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++rfs::sim::test_failures;                                                    \
        }                                                                                 \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                                      \
    do                                                                                             \
    {                                                                                              \
        const double check_a_ = static_cast<double>(a);                                            \
        const double check_b_ = static_cast<double>(b);                                            \
        if (!(std::fabs(check_a_ - check_b_) <= static_cast<double>(tol)))                         \
        {                                                                                          \
            std::fprintf(stderr, "%s:%d: CHECK_NEAR failed: %s = %g, %s = %g, tol %g\n", __FILE__, \
                __LINE__, #a, check_a_, #b, check_b_, static_cast<double>(tol));                   \
            ++rfs::sim::test_failures;                                                             \
        }                                                                                          \
    } while (0)

#define TEST_MAIN_RESULT(name)                                                           \
    (rfs::sim::test_failures ?                                                           \
            (std::fprintf(stderr, "%d check(s) failed\n", rfs::sim::test_failures), 1) : \
            (std::printf("all %s tests passed\n", name), 0))
