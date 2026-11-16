#pragma once

#include "effect.h"
#include "outlines.h"
#include "region.h"
#include "shape.h"
#include "textentry.h"
#include "transform.h"
#include "avgvisibility.h"

#include <pmr/heap.h>
#include <pmr/memory_resource.h>

#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace avg
{
    /**
     * @brief The flat, ordered list of elements a frame rasterises.
     *
     * Elements paint in list order. A drawing also carries outlines, the shapes its content casts shadows
     * with. They are not elements and the renderer never reads them, but
     * every operation that maps or combines drawings applies to them too.
     * Only the outermost drawing holds outlines (see liftSubDrawing()).
     */
    class AVG_EXPORT Drawing final
    {
    public:
        struct SubDrawing;
        struct ClipElement
        {
            pmr::heap<SubDrawing> subDrawing;
            Rect clipRect;
            Transform transform;
        };

        /**
         * @brief Content whose backdrop effects see only what was painted
         * before the scope.
         *
         * Effects directly inside the scope sample the frame as it was when
         * the scope was entered, never their siblings inside it. The root of
         * a drawing is an implicit scope. controlBb is the bounds of
         * subDrawing in its own space, strokes included, before transform.
         */
        struct BackdropScope
        {
            pmr::heap<SubDrawing> subDrawing;
            Rect controlBb;
            Transform transform;
        };

        struct RegionFill
        {
            Region region;
            Brush brush;
        };

        struct ShapeElement
        {
            Shape shape;
            std::optional<Brush> brush;
            std::optional<Pen> pen;
        };

        /**
         * @brief One input of an effect element: a sub-drawing, or the
         * backdrop of the enclosing scope when subDrawing is empty.
         *
         * controlBb is the sub-drawing's bounds in the element's space,
         * strokes included.
         */
        struct EffectInput
        {
            std::optional<pmr::heap<SubDrawing>> subDrawing;
            Rect controlBb;
        };

        /**
         * @brief Runs one pass over its inputs and draws the result.
         *
         * inputs line up with the pass's samplers. With a shape the pass
         * paints it; without one the pass covers its sub-drawing inputs'
         * content grown by its reach, at the scale the element is drawn
         * at, and its premultiplied result is composited over what is
         * below. A
         * sub-drawing input that holds only an element without a shape is
         * not drawn into a target of its own: that element's pass writes
         * straight into a target at its output's scale and format, which
         * is how passes nest into a multi-pass effect. The inputs and the
         * shape are in the element's space, which transform maps into the
         * drawing's space. controlBb, in the element's space, is the
         * shape's bounds, or else the sub-drawing inputs' bounds grown by
         * the pass's reach in logical units. Make one with Drawing::effect.
         */
        struct EffectElement
        {
            pmr::vector<EffectInput> inputs;
            ElementPass pass;
            std::optional<Shape> shape;
            Transform transform;
            Rect controlBb;
        };

        using Element = std::variant<ShapeElement, TextEntry, ClipElement,
              RegionFill, BackdropScope, EffectElement>;

        struct SubDrawing
        {
            pmr::vector<Element> elements;
        };

        Drawing(pmr::memory_resource* memory);
        Drawing(pmr::memory_resource* memory, Element element);

        Drawing(pmr::vector<Element> const& elements);
        Drawing(pmr::vector<Element>&& elements);

        ~Drawing();

        Drawing(Drawing const&) = default;
        Drawing(Drawing&&) noexcept = default;

        Drawing& operator=(Drawing const&) = default;
        Drawing& operator=(Drawing&&) noexcept = default;

        pmr::memory_resource* getResource() const;

        [[nodiscard]]
        Drawing operator+(Element&& element) &&;
        Drawing& operator+=(Element&& element);

        /**
         * @brief Appends the other drawing's elements and outlines to this
         * one's.
         */
        [[nodiscard]]
        Drawing operator+(Drawing const& drawing) &&;
        [[nodiscard]]
        Drawing operator+(Drawing&& drawing) &&;
        Drawing& operator+=(Drawing const& drawing);
        Drawing& operator+=(Drawing&& drawing);

        /**
         * @brief Wraps the elements into one BackdropScope; the outlines stay
         * on the result. An empty drawing stays empty.
         */
        [[nodiscard]]
        Drawing scope() &&;

        /**
         * @brief Paints above on top of below.
         *
         * The result is below's elements, then above's in a BackdropScope, so
         * above's backdrop effects see below but nothing of above itself.
         * The outlines of both end on the result. Use it wherever one child
         * is painted over another.
         */
        [[nodiscard]]
        static Drawing over(Drawing below, Drawing above);

        /**
         * @brief A drawing holding one EffectElement that runs pass over
         * inputs, painting shape when there is one.
         *
         * An empty input is the backdrop of the enclosing scope; any other
         * becomes a sub-drawing of the element, and its outlines move onto
         * the result. Throws std::invalid_argument when the
         * number of inputs is not the pass's, or when neither a shape nor a
         * sub-drawing input bounds the result.
         */
        [[nodiscard]]
        static Drawing effect(pmr::memory_resource* memory, ElementPass pass,
                std::vector<std::optional<Drawing>> inputs,
                std::optional<Shape> shape = std::nullopt);

        [[nodiscard]]
        Drawing operator*(float scale) &&;

        [[nodiscard]]
        Drawing clip(Rect const& r) &&;

        [[nodiscard]]
        Drawing clip(Obb const& obb) &&;

        pmr::vector<Element> const& getElements() const;
        Outlines const& getOutlines() const;

        [[nodiscard]]
        Drawing setOutlines(Outlines outlines) &&;
        Drawing& setOutlines(Outlines outlines) &;

        [[nodiscard]]
        Drawing addOutline(Shape shape) &&;
        Drawing& addOutline(Shape shape) &;

        [[nodiscard]]
        Drawing clearOutlines() &&;
        Drawing& clearOutlines() &;

        /**
         * @brief Moves the outlines out, leaving the drawing none.
         */
        Outlines takeOutlines();

        /**
         * @brief Turns @p inner into the sub-drawing of an element this
         * drawing will hold.
         *
         * inner's outlines, mapped by @p transform (the element's
         * transform), move onto this drawing.
         */
        [[nodiscard]]
        pmr::heap<SubDrawing> liftSubDrawing(Drawing&& inner,
                Transform const& transform);

        /**
         * @brief The bounds of the elements, not of the outlines.
         *
         * An effect element counts as its controlBb; the margin its passes
         * sample beyond that is added when the frame is rendered.
         */
        Rect getControlBb() const;

        [[nodiscard]]
        Drawing filterByRect(Rect const& r) &&;

        [[nodiscard]]
        Drawing transform(Transform const& t) &&;

        [[nodiscard]]
        Drawing translate(ase::Vector2f offset) &&;

        AVG_EXPORT friend Drawing operator*(Transform const& t, Drawing&& drawing);

    private:
        pmr::vector<Element> elements_;
        Outlines outlines_;
        Rect controlBb_;
    };
}

