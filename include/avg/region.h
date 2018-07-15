#pragma once

#include "endtype.h"
#include "jointype.h"
#include "fillrule.h"
#include "vector.h"

#include <btl/visibility.h>

#include <vector>
#include <memory>
#include <stdint.h>

namespace avg
{
    class SimplePolygon;
    class Rect;

    class RegionDeferred;

    class BTL_VISIBLE Region
    {
    public:
        Region();

        Region(std::vector<SimplePolygon> const& polygons, FillRule rule,
                Vector2f pixelSize, size_t resPerPixel);
        Region(std::vector<SimplePolygon> const& polygons, JoinType join,
                EndType end, float width, Vector2f pixelSize,
                size_t resPerPixel);
        Region(Region const&) = default;
        Region(Region&&) noexcept = default;

        ~Region();

        Region& operator=(Region const&) = default;
        Region& operator=(Region&&) noexcept = default;

        Region operator|(Region const& region) const;
        Region operator&(Region const& region) const;
        Region operator^(Region const& region) const;
        Region& operator|=(Region const& region) const;
        Region& operator&=(Region const& region) const;
        Region& operator^=(Region const& region) const;

        Region offset(JoinType join, EndType end, float offset) const;

        std::pair<std::vector<Vector2f>, std::vector<uint16_t> >
            triangulate() const;

        Region getClipped(Rect const& r) const;

    private:
        friend std::ostream& operator<<(std::ostream& stream,
                avg::Region const& region);

        void ensureUniqueness();

        inline RegionDeferred* d() { return deferred_.get(); }
        inline RegionDeferred const* d() const { return deferred_.get(); }
        std::shared_ptr<RegionDeferred> deferred_;
    };
}

