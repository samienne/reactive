#include "curve/curves.h"

#include <cmath>

// Curves inspired by https://easings.net/

namespace avg::curve
{

constexpr const float pi = 3.14159265358979323846f;

float linear(float f)
{
    return f;
}

float easeInCubic(float f)
{
    return f * f * f;
}

float easeOutCubic(float f)
{
    return 1.0f - easeInCubic(1.0f - f);
}

float easeInOutCubic(float f)
{
    return f < 0.5
        ? (4 * easeInCubic(f))
        : 1.0f - (std::pow(-2.0f * f + 2.0f, 3.0f) / 2.0f)
        ;
}

float easeInElastic(float f)
{
    constexpr float const c4 = (2.0f * pi) / 3.0f;

    return f == 0.0f
        ? 0.0f
        : f == 1.0f
        ? 1.0f
        : -std::pow(2.0f, 10.0f * f - 10.0f) * std::sin((f * 10.0f - 10.75f) * c4)
        ;
}

float easeOutElastic(float f)
{
    constexpr float const c4 = (2.0f * pi) / 3.0f;

    return f == 0.0f
        ? 0.0f
        : f == 1.0f
        ? 1.0f
        : std::pow(2.0f, -10.0f * f) * std::sin((f * 10.0f - 0.75f) * c4) + 1.0f
        ;
}

float easeInOutElastic(float f)
{
    constexpr float const c5 = (2.0f * pi) / 4.5f;

    return f == 0.0f
        ? 0.0f
        : f == 1.0f
        ? 1.0f
        : f < 0.5f
        ? -(std::pow(2.0f, 20.0f * f - 10.0f) * std::sin((20.0f * f - 11.125f) * c5)) / 2.0f
        : (std::pow(2.0f, -20.0f * f + 10.0f) * std::sin((20.0f * f - 11.125f) * c5)) / 2.0f + 1.0f
        ;
}

float easeInQuad(float f)
{
    return f * f;
}

float easeOutQuad(float f)
{
    return 1.0f - (1.0f - f) * (1.0f - f);
}

float easeInOutQuad(float f)
{
    return f < 0.5f
        ? 2.0f * f * f
        : 1 - std::pow(-2.0f * f + 2.0f, 2.0f) / 2.0f
        ;
}

float easeInBack(float f)
{
    constexpr float const c1 = 1.70158f;
    constexpr float const c3 = c1 + 1.0f;

    return c3 * f * f* f - c1 * f * f;
}

float easeOutBack(float f)
{
    constexpr float const c1 = 1.70158f;
    constexpr float const c3 = c1 + 1.0f;

    return 1.0f + c3 * std::pow(f - 1.0f, 3.0f) + c1 * std::pow(f - 1.0f, 2.0f);
}

float easeInOutBack(float f)
{
    constexpr float const c1 = 1.70158f;
    constexpr float const c2 = c1 * 1.525f;

    return f < 0.5f
        ? (std::pow(2.0f * f, 2.0f) * ((c2 + 1.0f) * 2.0f * f - c2)) / 2.0f
        : (std::pow(2.0f * f - 2.0f, 2.0f) * ((c2 + 1.0f) * (f * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f
                ;
}

float easeInBounce(float f)
{
    return 1.0f - easeOutBounce(1.0f - f);
}

float easeOutBounce(float f)
{
    constexpr float const n1 = 7.5625f;
    constexpr float const d1 = 2.75f;

    if (f < 1.0f / d1)
        return n1 * f * f;
    else if (f < 2.0f / d1)
        return n1 * (f - 1.5f / d1) * (f - 1.5f / d1) + 0.75f;
    else if (f < 2.5f / d1)
        return n1 * (f - 2.25f / d1) * (f - 2.25f / d1) + 0.9375f;
    else
        return n1 * (f - 2.625f / d1) * (f - 2.625f / d1) + 0.984375f;
}

float easeInOutBounce(float f)
{
    return f < 0.5f
        ? (1 - easeOutBounce(1.0f - 2.0f * f)) / 2.0f
        : (1 + easeOutBounce(2.0f * f - 1.0f)) / 2.0f
        ;
}

} // namespace avg::curve
