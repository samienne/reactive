#pragma once

#include "avgvisibility.h"

#include <ase/format.h>
#include <ase/shaderinterface.h>

#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace avg
{
    /**
     * @brief How many times smaller than the screen a pass's target is.
     *
     * A target at Divisor::Four covers the screen with a quarter of the
     * pixels per axis.
     */
    enum class Divisor
    {
        One = 1,
        Two = 2,
        Four = 4,
        Eight = 8
    };

    AVG_EXPORT int toInt(Divisor divisor);

    AVG_EXPORT bool isValid(Divisor divisor);

    AVG_EXPORT std::ostream& operator<<(std::ostream& stream, Divisor divisor);

    /**
     * @brief Where a pass reads from: the effect's backdrop or the output
     * of an earlier pass of the same effect.
     *
     *     PassInput::backdrop()
     *     PassInput::pass(0)
     *
     * The backdrop is the snapshot of everything drawn before the scope the
     * effect sits in; pass indices count from the first pass of the effect.
     */
    class AVG_EXPORT PassInput
    {
    public:
        static PassInput backdrop();
        static PassInput pass(size_t index);

        bool isBackdrop() const;

        /**
         * @brief The producing pass; only meaningful when not backdrop.
         */
        size_t getPass() const;

        bool operator==(PassInput const& rhs) const;
        bool operator!=(PassInput const& rhs) const;

    private:
        std::optional<size_t> pass_;
    };

    AVG_EXPORT std::ostream& operator<<(std::ostream& stream,
            PassInput const& input);

    /**
     * @brief The target a pass renders into: a scratch texture at a
     * divisor of the screen size, in a colour format.
     */
    struct AVG_EXPORT PassOutput
    {
        Divisor divisor = Divisor::One;
        ase::Format format = ase::FORMAT_SRGBA;

        bool operator==(PassOutput const& rhs) const;
        bool operator!=(PassOutput const& rhs) const;
    };

    /**
     * @brief What a pass draws: a quad over its region, or the effect's
     * shape. The shape is drawn only by the final pass, into the main
     * target.
     */
    enum class Geometry
    {
        Region,
        Shape
    };

    AVG_EXPORT std::ostream& operator<<(std::ostream& stream,
            Geometry geometry);

    /**
     * @brief A fragment shader and the interface it is compiled against.
     *
     * The vertex shader is fixed per Geometry. The interface names the
     * samplers the pass's inputs bind to, in input order, and the uniform
     * block the pass's parameters are written into.
     */
    struct AVG_EXPORT PassShader
    {
        std::string fragmentSource;
        ase::ShaderInterface interface;
    };

    /**
     * @brief One step of an effect: a shader run over the inputs into an
     * output.
     *
     * padding is how far outside its own region, in pixels of its output,
     * the pass samples its inputs (a blur reads its radius beyond every
     * edge); the planner grows the inputs' regions by it. A Region pass
     * needs an output; the final Shape pass has none, since it draws into
     * the main target.
     */
    struct AVG_EXPORT Pass
    {
        PassShader shader;
        std::vector<PassInput> inputs;
        std::optional<PassOutput> output;
        Geometry geometry = Geometry::Region;
        int padding = 0;
    };

    /**
     * @brief Lists every rule the pass list breaks; empty when it is a
     * well-formed effect.
     *
     * The rules: there is at least one pass; the last pass, and only the
     * last, has Shape geometry; every input is the backdrop or an earlier
     * pass, so no pass reads its own output; every Region pass has an
     * output with a Divisor of 1, 2, 4 or 8 and a format of FORMAT_SRGBA,
     * FORMAT_R8 or FORMAT_RGBA16F; the Shape pass has no output; every
     * Region pass's output is read by a later pass; no padding is negative.
     */
    AVG_EXPORT std::vector<std::string> validate(
            std::vector<Pass> const& passes);

    /**
     * @brief A directed acyclic graph of passes that ends by drawing a
     * shape into the main target.
     *
     *     avg::Effect blur({ downsample, blurH, blurV, tint });
     *
     * The passes are in execution order and each reads only the backdrop
     * or passes before it. Construction throws std::invalid_argument
     * listing every rule the passes break; validate() reports the same
     * list without throwing.
     */
    class AVG_EXPORT Effect
    {
    public:
        explicit Effect(std::vector<Pass> passes);
        Effect(Effect const&) = default;
        Effect(Effect&&) noexcept = default;
        ~Effect();

        Effect& operator=(Effect const&) = default;
        Effect& operator=(Effect&&) noexcept = default;

        std::vector<Pass> const& getPasses() const;

        /**
         * @brief The Shape pass, always the last one.
         */
        Pass const& getFinalPass() const;

    private:
        std::vector<Pass> passes_;
    };
}
