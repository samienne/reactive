#pragma once

#include "region.h"
#include "shape.h"
#include "textentry.h"
#include "transform.h"
#include "avgvisibility.h"

#include <pmr/heap.h>
#include <pmr/memory_resource.h>

#include <memory>
#include <variant>

namespace avg
{
    class Effect;

    /**
     * @brief The flat, ordered list of elements a frame rasterises.
     *
     * Elements paint in list order. Besides that main list a drawing carries
     * pending shadows: elements that belong under the content of the
     * container that will eventually hold them. Pending shadows ride along
     * through concatenation, clipping and transforms, and flush() moves them
     * ahead of the main list. Whoever renders a drawing flushes it first: an
     * unflushed drawing's shadows are silently dropped.
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
         * @brief A backdrop effect drawn through shape.
         *
         * The effect's final pass paints shape; the passes before it sample
         * the enclosing scope's backdrop around shape's bounds. shape is in
         * the element's own space and transform maps it, together with the
         * effect's logical units, into the drawing's space. Build the
         * effect once and share the pointer across elements and frames.
         */
        struct EffectElement
        {
            Shape shape;
            std::shared_ptr<Effect const> effect;
            Transform transform;
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
         * @brief Appends the other drawing's elements and pending shadows to
         * this one's.
         */
        [[nodiscard]]
        Drawing operator+(Drawing const& drawing) &&;
        Drawing& operator+=(Drawing const& drawing);

        /**
         * @brief Adds a pending shadow, an element painted under the content
         * of the enclosing container once the drawing is flushed.
         */
        [[nodiscard]]
        Drawing addShadow(Element&& shadow) &&;
        Drawing& addShadow(Element&& shadow) &;

        /**
         * @brief Moves the pending shadows ahead of the main list.
         */
        [[nodiscard]]
        Drawing flush() &&;

        /**
         * @brief Wraps the main list into one BackdropScope. Pending shadows
         * stay pending, outside the scope. An empty main list stays empty.
         */
        [[nodiscard]]
        Drawing scope() &&;

        /**
         * @brief Paints above on top of below.
         *
         * The result is below's main list, then above's pending shadows, then
         * above's main list in a BackdropScope, so above's backdrop effects
         * see below and the shadows but nothing of above itself. below's
         * pending shadows stay pending. Use it wherever one child is painted
         * over another.
         */
        [[nodiscard]]
        static Drawing over(Drawing below, Drawing above);

        [[nodiscard]]
        Drawing operator*(float scale) &&;

        [[nodiscard]]
        Drawing clip(Rect const& r) &&;

        [[nodiscard]]
        Drawing clip(Obb const& obb) &&;

        pmr::vector<Element> const& getElements() const;
        pmr::vector<Element> const& getPendingShadows() const;

        /**
         * @brief The bounds of the elements and the pending shadows.
         *
         * An effect element counts as its shape's bounds; the margin its
         * passes sample beyond them is added when the frame is rendered.
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
        pmr::vector<Element> shadows_;
        Rect controlBb_;
    };
}

