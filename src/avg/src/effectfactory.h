#pragma once

#include <avg/effect.h>
#include <avg/avgvisibility.h>

#include <memory>
#include <optional>
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

        // The effect with every size multiplied by factor.
        virtual Effect scaled(float factor) const = 0;
    };

    // What a factory keeps in an element pass so the renderer can convert
    // its sizes to pixels.
    class AVG_EXPORT ElementPassRecipe
    {
    public:
        virtual ~ElementPassRecipe();

        // The pass with every size multiplied by factor.
        virtual ElementPass scaled(float factor) const = 0;
    };

    // The construction route for factories and tests. Throws
    // std::invalid_argument listing every rule the passes break. An effect
    // without a recipe interpolates by switching.
    AVG_EXPORT Effect makeEffect(std::vector<Pass> passes,
            std::shared_ptr<EffectRecipe const> recipe = nullptr);

    // The element-pass route. pass needs an output and Region geometry and
    // no inputs; its shader declares one sampler per input of the element.
    // reach defaults to the padding in both axes and must lie within it.
    // Throws std::invalid_argument listing every rule it breaks.
    AVG_EXPORT ElementPass makeElementPass(Pass pass,
            std::shared_ptr<ElementPassRecipe const> recipe = nullptr,
            std::optional<Vector2i> reach = std::nullopt);

    // The effect with its sizes, given in logical units, in pixels of a
    // target at scalingFactor. An effect without a recipe is returned as it
    // is, its sizes taken to be pixels already.
    AVG_EXPORT Effect toPixels(Effect const& effect, float scalingFactor);

    AVG_EXPORT ElementPass toPixels(ElementPass const& pass,
            float scalingFactor);
}
