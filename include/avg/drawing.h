#pragma once

#include "shape.h"
#include "textentry.h"
#include "transform.h"
#include "avgvisibility.h"

#include <btl/variant.h>
#include <btl/heap.h>

namespace avg
{
    class AVG_EXPORT Drawing final
    {
    public:
        struct SubDrawing;
        struct ClipElement
        {
            btl::Heap<SubDrawing> subDrawing;
            Rect clipRect;
            Transform transform;

            bool operator==(ClipElement const& rhs) const
            {
                return subDrawing == rhs.subDrawing
                    && clipRect == rhs.clipRect
                    && transform == rhs.transform
                    ;
            }
        };

        using Element = btl::variant<Shape, TextEntry, ClipElement>;

        struct SubDrawing
        {
            std::vector<Element> elements;
        };

        friend btl::Heap<SubDrawing> operator*(btl::Heap<SubDrawing> const& s, float)
        {
            return s;
        }

        friend btl::Heap<SubDrawing> operator+(btl::Heap<SubDrawing> const& s, Vector2f)
        {
            return s;
        }

        Drawing();
        Drawing(Element element);
        Drawing(std::vector<Element> const& elements);
        Drawing(std::vector<Element>&& elements);

        ~Drawing();

        Drawing(Drawing const&) = default;
        Drawing(Drawing&&) noexcept = default;

        Drawing& operator=(Drawing const&) = default;
        Drawing& operator=(Drawing&&) noexcept = default;

        Drawing operator+(Element&& element) &&;
        Drawing& operator+=(Element&& element);

        Drawing operator+(Drawing const& drawing) &&;
        Drawing& operator+=(Drawing const& drawing);

        Drawing operator*(float scale) &&;
        Drawing operator+(ase::Vector2f offset) &&;

        bool operator==(Drawing const& rhs) const;
        bool operator!=(Drawing const& rhs) const;

        Drawing clip(Rect const& r) &&;
        Drawing clip(Obb const& obb) &&;

        std::vector<Element> const& getElements() const;
        Rect getControlBb() const;

        Drawing filterByRect(Rect const& r) &&;

        [[nodiscard]]
        Drawing transform(Transform const& t) &&;

        AVG_EXPORT friend Drawing operator*(Transform const& t, Drawing&& drawing);

    private:
        std::vector<Element> elements_;
        Rect controlBb_;
    };
}

