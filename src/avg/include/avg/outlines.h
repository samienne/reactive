#pragma once

#include "obb.h"
#include "rect.h"
#include "shape.h"
#include "transform.h"
#include "avgvisibility.h"

#include <pmr/memory_resource.h>
#include <pmr/vector.h>

namespace avg
{
    /**
     * @brief The outline shapes of the widgets that produced a drawing.
     *
     * A Drawing holds one next to its elements and applies every transform
     * and clip to both, so `outlines.transform(t).clip(obb)` keeps what of
     * each outline is still visible. The renderer never reads them. The
     * shapes are kept apart; nothing unions them.
     */
    class AVG_EXPORT Outlines final
    {
    public:
        explicit Outlines(pmr::memory_resource* memory);
        Outlines(pmr::memory_resource* memory, Shape shape);

        Outlines(Outlines const&) = default;
        Outlines(Outlines&&) noexcept = default;

        Outlines& operator=(Outlines const&) = default;
        Outlines& operator=(Outlines&&) noexcept = default;

        pmr::memory_resource* getResource() const;

        Outlines& operator+=(Shape shape);
        Outlines& operator+=(Outlines const& outlines);
        Outlines& operator+=(Outlines&& outlines);

        [[nodiscard]]
        Outlines operator+(Outlines const& outlines) &&;

        /**
         * @brief Cuts each outline to @p r and drops those outside it.
         */
        [[nodiscard]]
        Outlines clip(Rect const& r) &&;

        /**
         * @brief Cuts each outline to @p obb and drops those outside it.
         */
        [[nodiscard]]
        Outlines clip(Obb const& obb) &&;

        /**
         * @brief Drops the outlines whose bounds do not overlap @p r.
         */
        [[nodiscard]]
        Outlines filterByRect(Rect const& r) &&;

        [[nodiscard]]
        Outlines transform(Transform const& t) &&;

        [[nodiscard]]
        Outlines operator*(float scale) &&;

        pmr::vector<Shape> const& getShapes() const;
        bool isEmpty() const;

        /**
         * @brief The union of the outlines' bounds.
         */
        Rect getControlBb() const;

    private:
        pmr::vector<Shape> shapes_;
    };
} // namespace avg
