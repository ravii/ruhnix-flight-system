#include <cmath>
#include <cstdio>

#include "rfs/localization/pose_estimator.hpp"

namespace
{
int failures = 0;
#define CHECK(cond)                                                                       \
    do                                                                                    \
    {                                                                                     \
        if (!(cond))                                                                      \
        {                                                                                 \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

using rfs::localization::PoseEstimator;

void test_predict_constant_acceleration()
{
    PoseEstimator est;
    for (int i = 0; i < 100; ++i)
    {
        est.predict({1.0f, 0.0f, 0.0f}, 0.01f);  // 1 s at 1 m/s^2
    }
    CHECK(std::fabs(est.velocity()[0] - 1.0f) < 1e-3f);
    CHECK(std::fabs(est.position()[0] - 0.5f) < 1e-3f);
}

void test_converges_to_position_fixes()
{
    PoseEstimator est;
    for (int i = 0; i < 200; ++i)
    {
        est.predict({0.0f, 0.0f, 0.0f}, 0.01f);
        est.correct_position({10.0f, -5.0f, 2.0f}, 0.25f);
    }
    CHECK(std::fabs(est.position()[0] - 10.0f) < 0.05f);
    CHECK(std::fabs(est.position()[1] + 5.0f) < 0.05f);
    CHECK(std::fabs(est.position()[2] - 2.0f) < 0.05f);
    CHECK(est.position_variance()[0] < 0.25f);  // fused estimate beats a single fix
}
}  // namespace

int main()
{
    test_predict_constant_acceleration();
    test_converges_to_position_fixes();
    if (failures)
    {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all localization tests passed\n");
    return 0;
}
