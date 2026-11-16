#pragma once

#include <avg/effect.h>
#include <avg/avgvisibility.h>

#include <memory>
#include <vector>

namespace avg
{
    // What a factory keeps in its effects so that two of the same kind can
    // be interpolated: the parameters the passes were built from.
    class AVG_EXPORT EffectRecipe
    {
    public:
        virtual ~EffectRecipe();

        virtual bool isSameKind(EffectRecipe const& rhs) const = 0;

        // Only called with a recipe of the same kind.
        virtual Effect interpolate(EffectRecipe const& rhs, float t) const = 0;
    };

    // The construction route for factories and tests. Throws
    // std::invalid_argument listing every rule the passes break. An effect
    // without a recipe interpolates by switching.
    AVG_EXPORT Effect makeEffect(std::vector<Pass> passes,
            std::shared_ptr<EffectRecipe const> recipe = nullptr);
}
