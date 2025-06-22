#pragma once

#include "avg/avgvisibility.h"

namespace avg::curve
{
    AVG_EXPORT float linear(float f);
    AVG_EXPORT float easeInCubic(float f);
    AVG_EXPORT float easeOutCubic(float f);
    AVG_EXPORT float easeInOutCubic(float f);
    AVG_EXPORT float easeInElastic(float f);
    AVG_EXPORT float easeOutElastic(float f);
    AVG_EXPORT float easeInOutElastic(float f);
    AVG_EXPORT float easeInQuad(float f);
    AVG_EXPORT float easeOutQuad(float f);
    AVG_EXPORT float easeInOutQuad(float f);
    AVG_EXPORT float easeInBack(float f);
    AVG_EXPORT float easeOutBack(float f);
    AVG_EXPORT float easeInOutBack(float f);
    AVG_EXPORT float easeInBounce(float f);
    AVG_EXPORT float easeOutBounce(float f);
    AVG_EXPORT float easeInOutBounce(float f);
} // namespace reactive::curve

