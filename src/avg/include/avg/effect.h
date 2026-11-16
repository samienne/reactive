#pragma once

#include "vector.h"
#include "avgvisibility.h"

#include <ase/format.h>
#include <ase/shaderinterface.h>
#include <ase/uniformvalues.h>

#include <cstddef>
#include <cstdint>
#include <memory>
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
     * @brief A cheap value standing for a pass shader's source.
     *
     * create() returns an identity no other call returns; a
     * default-constructed one stands for no shader in particular. Passes
     * compare their shaders by identity, and by source as well when the
     * identity is the default one.
     */
    class AVG_EXPORT ShaderId
    {
    public:
        static ShaderId create();

        bool operator==(ShaderId const& rhs) const;
        bool operator!=(ShaderId const& rhs) const;

    private:
        uint64_t value_ = 0;
    };

    /**
     * @brief A fragment shader, the interface it is compiled against and
     * the identity that stands for both.
     *
     * The vertex shader is fixed per Geometry. The interface names the
     * samplers the pass's inputs bind to, in input order, and the uniform
     * block the pass's parameters are written into.
     */
    struct AVG_EXPORT PassShader
    {
        ShaderId id;
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
     * the main target. parameters are the contents of one uniform block
     * the shader's interface declares, uploaded as they are for every
     * draw of the pass; a pass whose shader takes none leaves it empty.
     */
    struct AVG_EXPORT Pass
    {
        PassShader shader;
        std::vector<PassInput> inputs;
        std::optional<PassOutput> output;
        Geometry geometry = Geometry::Region;
        int padding = 0;
        std::optional<ase::UniformValues> parameters;

        /**
         * @brief Equal when the shaders are (see ShaderId) and the inputs,
         * output, geometry, padding and parameter bytes match.
         */
        bool operator==(Pass const& rhs) const;
        bool operator!=(Pass const& rhs) const;
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
     * Region pass's output is read by a later pass; no padding is negative;
     * a pass has at most maxPassInputs inputs and its shader's interface
     * declares one sampler per input; a pass's parameters, when present,
     * fill a block its shader's interface declares with that name and
     * layout.
     */
    AVG_EXPORT std::vector<std::string> validate(
            std::vector<Pass> const& passes);

    /**
     * @brief A directed acyclic graph of passes that ends by drawing a
     * shape into the main target.
     *
     *     avg::Effect glass = avg::frostedGlass(options);
     *
     * An effect is an opaque value: the factories in builtineffects.h are
     * the only way to make one, and none needs a GPU context. The passes
     * are in execution order and each reads only the backdrop or passes
     * before it. Copies share the passes, and a move copies, so a
     * moved-from effect is still the effect it was. Two effects are equal
     * when their passes are, pass by pass.
     */
    class AVG_EXPORT Effect
    {
    public:
        Effect(Effect const&) = default;
        ~Effect();

        Effect& operator=(Effect const&) = default;

        std::vector<Pass> const& getPasses() const;

        /**
         * @brief The Shape pass, always the last one.
         */
        Pass const& getFinalPass() const;

        bool operator==(Effect const& rhs) const;
        bool operator!=(Effect const& rhs) const;

    private:
        friend class EffectAccess;

        struct Content;

        explicit Effect(std::shared_ptr<Content const> content);

        std::shared_ptr<Content const> content_;
    };

    AVG_EXPORT std::ostream& operator<<(std::ostream& stream,
            Effect const& effect);

    /**
     * @brief The one pass an effect element runs over its inputs.
     *
     * An opaque value from a factory, like Effect. getPass() holds the
     * shader, the padding, the parameters and the output: the scale and
     * format of the element's result when another element reads it. Its
     * inputs are empty and its geometry is Region. Copies share the pass;
     * two are equal when their passes are.
     */
    class AVG_EXPORT ElementPass
    {
    public:
        ElementPass(ElementPass const&) = default;
        ~ElementPass();

        ElementPass& operator=(ElementPass const&) = default;

        Pass const& getPass() const;

        /**
         * @brief How many inputs the pass reads: the samplers its shader
         * declares.
         */
        size_t getInputCount() const;

        /**
         * @brief How far the result reaches past the content of the
         * element's sub-drawing inputs, per axis, in pixels of the output.
         *
         * The padding in both axes unless the factory gives less, as a
         * blur in one direction does.
         */
        Vector2i getReach() const;

        bool operator==(ElementPass const& rhs) const;
        bool operator!=(ElementPass const& rhs) const;

    private:
        friend class EffectAccess;

        struct Content;

        explicit ElementPass(std::shared_ptr<Content const> content);

        std::shared_ptr<Content const> content_;
    };

    /**
     * @brief True when a and b come from the same factory, so lerp()
     * interpolates their parameters.
     */
    AVG_EXPORT bool isSameKind(Effect const& a, Effect const& b);

    /**
     * @brief Interpolates the parameters of two effects of the same kind
     * by t; any other pair switches to b at once.
     *
     * A t outside [0, 1], as an overshooting curve gives, carries the
     * parameters out of the range the factory accepts; the passes are
     * built from the parameters clamped back into it.
     */
    AVG_EXPORT Effect lerp(Effect const& a, Effect const& b, float t);
}
