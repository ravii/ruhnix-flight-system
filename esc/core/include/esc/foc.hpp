/** Field-oriented control transforms. Platform-independent; no RTOS/hardware deps. */
#pragma once

namespace esc
{

struct Abc
{
    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;
};

struct AlphaBeta
{
    float alpha = 0.0f;
    float beta = 0.0f;
};

struct Dq
{
    float d = 0.0f;
    float q = 0.0f;
};

/** Amplitude-invariant Clarke transform (assumes a + b + c = 0). */
AlphaBeta clarke(const Abc& in);

/** Park transform; theta is the electrical angle in radians. */
Dq park(const AlphaBeta& in, float theta);

AlphaBeta inv_park(const Dq& in, float theta);

}  // namespace esc
