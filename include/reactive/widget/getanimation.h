#pragma once

#include "setanimation.h"
#include "getparam.h"

namespace reactive::widget
{
    inline auto getAnimation()
    {
        return getParam<AnimationTag>();
    }
} // namespace reactive::widget

