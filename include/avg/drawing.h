#pragma once

#include "region.h"
#include "shape.h"
#include "textentry.h"
#include "transform.h"
#include "avgvisibility.h"

#include <pmr/heap.h>
#include <pmr/memory_resource.h>

#include <variant>

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
        };

        struct RegionFill
        {
            Region region;
            Brush brush;
        };

        using Element = std::variant<Shape, TextEntry, ClipElement, RegionFill>;

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

        [[nodiscard]]
        Drawing operator+(Drawing const& drawing) &&;
        Drawing& operator+=(Drawing const& drawing);

        [[nodiscard]]
        Drawing operator*(float scale) &&;

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

        [[nodiscard]]
        Drawing translate(ase::Vector2f offset) &&;

        AVG_EXPORT friend Drawing operator*(Transform const& t, Drawing&& drawing);

    private:
        pmr::vector<Element> elements_;
        Rect controlBb_;
    };
}

