#include <cmath>
#include <cstdio>

#include "esc/current_controller.hpp"
#include "esc/foc.hpp"

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
#define NEAR(a, b) (std::fabs((a) - (b)) < 1e-4f)

constexpr float kTwoPiOver3 = 2.09439510239319549f;

esc::Abc balanced(float amplitude, float theta)
{
    return {amplitude * std::cos(theta), amplitude * std::cos(theta - kTwoPiOver3),
        amplitude * std::cos(theta + kTwoPiOver3)};
}

/** A balanced three-phase current aligned with theta maps to pure d-axis. */
void test_balanced_currents_map_to_d_axis()
{
    for (int k = 0; k < 16; ++k)
    {
        const float theta = static_cast<float>(k) * 0.4f;
        const esc::Dq dq = esc::park(esc::clarke(balanced(1.0f, theta)), theta);
        CHECK(NEAR(dq.d, 1.0f));
        CHECK(NEAR(dq.q, 0.0f));
    }
}

void test_park_roundtrip()
{
    const esc::AlphaBeta ab{0.3f, -0.7f};
    const esc::AlphaBeta back = esc::inv_park(esc::park(ab, 1.234f), 1.234f);
    CHECK(NEAR(back.alpha, ab.alpha));
    CHECK(NEAR(back.beta, ab.beta));
}

void test_current_loop_zero_error_zero_voltage()
{
    esc::CurrentController ctl;
    esc::CurrentController::Input in;
    in.theta_elec = 0.7f;
    const esc::AlphaBeta v = ctl.update(in, 50e-6f);
    CHECK(NEAR(v.alpha, 0.0f) && NEAR(v.beta, 0.0f));
}

void test_current_loop_q_error_drives_q_voltage()
{
    esc::CurrentController ctl;
    esc::CurrentController::Input in;
    in.theta_elec = 1.1f;
    in.current_ref_a.q = 2.0f;
    const esc::AlphaBeta v = ctl.update(in, 50e-6f);
    const esc::Dq v_dq = esc::park(v, in.theta_elec);
    CHECK(v_dq.q > 0.0f);
    CHECK(NEAR(v_dq.d, 0.0f));
}

}  // namespace

int main()
{
    test_balanced_currents_map_to_d_axis();
    test_park_roundtrip();
    test_current_loop_zero_error_zero_voltage();
    test_current_loop_q_error_drives_q_voltage();
    if (failures)
    {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all esc core tests passed\n");
    return 0;
}
