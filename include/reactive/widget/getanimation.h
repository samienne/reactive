#pragma once

#include "getparam.h"

#include <avg/animationoptions.h>

namespace reactive::widget
{
    struct AnimationTag
    {
        using type = std::optional<avg::AnimationOptions>;
        static AnySharedSignal<type> getDefaultValue()
        {
            return share(signal::constant<typename AnimationTag::type>(std::nullopt));
        }
    };

    inline auto getAnimation()
    {
        return getParam<AnimationTag>();
    }
} // namespace reactive::widget

