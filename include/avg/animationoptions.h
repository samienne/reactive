#pragma once

#include "curve.h"
#include "avgvisibility.h"

#include <chrono>

namespace avg
{
    struct AVG_EXPORT AnimationOptions
    {
        std::chrono::milliseconds duration;
        Curve curve;
    };
} // namespace avg
