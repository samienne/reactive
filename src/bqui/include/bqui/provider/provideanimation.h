#pragma once

#include "provideparam.h"

#include <bq/signal/signal.h>

#include <avg/animationoptions.h>

#include <optional>

namespace bqui::provider
{
    struct AnimationTag
    {
        using type = std::optional<avg::AnimationOptions>;
        static bq::signal::AnySignal<type> getDefaultValue()
        {
            return bq::signal::constant<typename AnimationTag::type>(std::nullopt);
        }
    };

    inline auto provideAnimation()
    {
        return provideParam<AnimationTag>();
    }
} // namespace reactive::widget

