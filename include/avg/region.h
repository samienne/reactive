#pragma once

#include "transform.h"
#include "endtype.h"
#include "jointype.h"
#include "fillrule.h"
#include "vector.h"
#include "avgvisibility.h"
#include "operationtype.h"

#include <pmr/vector.h>
#include <pmr/memory_resource.h>

#include <memory>
#include <stdint.h>

namespace avg
{
    class PolyLine;
    class Rect;

    class RegionDeferred;

    class AVG_EXPORT Region
    {
    public:
        explicit Region(pmr::memory_resource* memory);

        Region(pmr::memory_resource* memory,
                pmr::vector<PolyLine> const& polygons, FillRule rule,
                Vector2f pointSize);
        Region(pmr::memory_resource* memory,
                pmr::vector<PolyLine> const& polygons, JoinType join,
                EndType end, float width, Vector2f pointSize);
        Region(pmr::memory_resource* memory,
                OperationType operation,
                pmr::vector<PolyLine> const& lhs,
                pmr::vector<PolyLine> const& rhs,
                FillRule rule,
                Vector2f pointSize);

        Region(Region const&) = default;
        Region(Region&&) noexcept = default;

        ~Region();

        Region& operator=(Region const&) = default;
        Region& operator=(Region&&) noexcept = default;

        Region operator|(Region const& region) const;
        Region operator&(Region const& region) const;
        Region operator-(Region const& region) const;
        Region operator^(Region const& region) const;
        Region& operator|=(Region const& region);
        Region& operator&=(Region const& region);
        Region& operator-=(Region const& region);
        Region& operator^=(Region const& region);

        Region offset(JoinType join, EndType end, float offset) const;

        std::pair<pmr::vector<Vector2f>, pmr::vector<uint32_t> >
            triangulate(pmr::memory_resource* memory) const;

        Region getClipped(Rect const& r) const;

        Rect getBoundingBox() const;
        pmr::memory_resource* getResource() const;
        Region withResource(pmr::memory_resource* memory) const;

        friend Region operator*(avg::Transform const& t, Region&& r);

    private:
        static Region operateRegions(
                pmr::memory_resource* memory,
                Region const& lhs,
                Region const& rhs,
                OperationType operation);

    private:
        friend std::ostream& operator<<(std::ostream& stream,
                avg::Region const& region);

        void ensureUniqueness();

        inline RegionDeferred* d() { return deferred_.get(); }
        inline RegionDeferred const* d() const { return deferred_.get(); }

        pmr::memory_resource* memory_;
        std::shared_ptr<RegionDeferred> deferred_;
    };
}

