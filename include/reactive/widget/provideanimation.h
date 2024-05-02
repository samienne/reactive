#pragma once

#include "provideparam.h"

#include <avg/animationoptions.h>

namespace reactive::widget
{
    struct AnimationTag
    {
        using type = std::optional<avg::AnimationOptions>;
        static signal2::AnySignal<type> getDefaultValue()
        {
            return signal2::constant<typename AnimationTag::type>(std::nullopt);
        }
    };

    inline auto provideAnimation()
    {
        return provideParam<AnimationTag>();
    }
} // namespace reactive::widget

