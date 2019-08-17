#pragma once

#include "shape.h"
#include "textentry.h"
#include "transform.h"
#include "avgvisibility.h"

#include <btl/variant.h>

#include <pmr/heap.h>
#include <pmr/memory_resource.h>

namespace avg
{
    class AVG_EXPORT Drawing final
    {
    public:
        struct SubDrawing;
        struct ClipElement
        {
            pmr::heap<SubDrawing> subDrawing;
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
            pmr::vector<Element> elements;
        };

        friend pmr::heap<SubDrawing> operator*(pmr::heap<SubDrawing> const& s, float)
        {
            return s;
        }

        friend pmr::heap<SubDrawing> operator+(pmr::heap<SubDrawing> const& s, Vector2f)
        {
            return s;
        }

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

        Drawing operator+(Element&& element) &&;
        Drawing& operator+=(Element&& element);

        Drawing operator+(Drawing const& drawing) &&;
        Drawing& operator+=(Drawing const& drawing);

        Drawing operator*(float scale) &&;
        Drawing operator+(ase::Vector2f offset) &&;

        bool operator==(Drawing const& rhs) const;
        bool operator!=(Drawing const& rhs) const;

        [[nodiscard]]
        Drawing clip(Rect const& r) &&;

        [[nodiscard]]
        Drawing clip(Obb const& obb) &&;

        pmr::vector<Element> const& getElements() const;
        Rect getControlBb() const;

        [[nodiscard]]
        Drawing filterByRect(Rect const& r) &&;

        [[nodiscard]]
        Drawing transform(Transform const& t) &&;

        AVG_EXPORT friend Drawing operator*(Transform const& t, Drawing&& drawing);

    private:
        pmr::vector<Element> elements_;
        Rect controlBb_;
    };
}

