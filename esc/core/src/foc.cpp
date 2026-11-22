#include "esc/foc.hpp"

#include <cmath>

namespace esc
{
namespace
{

constexpr float kInvSqrt3 = 0.57735026918962576f;

}  // namespace

AlphaBeta clarke(const Abc& in)
{
    return {in.a, (in.a + 2.0f * in.b) * kInvSqrt3};
}

Dq park(const AlphaBeta& in, float theta)
{
    const float s = std::sin(theta);
    const float c = std::cos(theta);
    return {in.alpha * c + in.beta * s, -in.alpha * s + in.beta * c};
}

AlphaBeta inv_park(const Dq& in, float theta)
{
    const float s = std::sin(theta);
    const float c = std::cos(theta);
    return {in.d * c - in.q * s, in.d * s + in.q * c};
}

}  // namespace esc
