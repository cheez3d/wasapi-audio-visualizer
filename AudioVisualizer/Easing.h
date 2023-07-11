#pragma once

#include <SDL.h>

#include <cmath>

inline float EaseLin(float x)
{
    return x;
}

inline float EaseInCirc(float x)
{
    return 1.f - std::sqrtf(1.f - std::powf(x, 2.f));
}

inline float EaseOutCirc(float x)
{
    return std::sqrtf(1.f - std::powf(x - 1.f, 2.f));
}

inline float EaseInExp(float x)
{
    return x <= 0.f ? 0.f : std::powf(2.f, 10.f * x - 10.f);
}

inline float EaseOutExp(float x)
{
    return x >= 1.f ? 1.f : 1.f - std::powf(2.f, -10.f * x);
}

inline float EaseInSine(float x)
{
    return 1.f - std::cosf(x * (float)M_PI / 2.f);
}

inline float EaseOutSine(float x)
{
    return std::sinf(x * (float)M_PI / 2.f);
}

inline float Lerp(float x, float a, float b)
{
    return (1.f - x) * a + x * b;
}
