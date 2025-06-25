#pragma once

#include "paramprovider.h"

#include "bqui/buildparams.h"

namespace bqui::provider
{
    inline auto provideBuildParams()
    {
        return makeParamProviderUnchecked([](BuildParams const& params)
                -> BuildParams const&
            {
                return params;
            });
    }
} // namespace reactive::widget
