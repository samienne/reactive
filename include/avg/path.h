#pragma once

#include "endtype.h"
#include "jointype.h"
#include "fillrule.h"
#include "transform.h"
#include "vector.h"
#include "rect.h"
#include "avgvisibility.h"

#include <btl/hash.h>

#include <pmr/memory_resource.h>

#include <vector>
#include <memory>

namespace avg
{
    class Region;
    class PathDeferred;
    class Transform;

    class AVG_EXPORT Path final
    {
    public:
        enum SegmentType
        {
            SEGMENT_START = 0,
            SEGMENT_LINE,
            SEGMENT_CONIC,
            SEGMENT_CUBIC,
            SEGMENT_ARC
        };

        Path();
        Path(Path const&) = default;
        Path(Path&&) noexcept = default;
        ~Path();

        bool operator==(Path const& rhs) const;
        bool operator!=(Path const& rhs) const;

        Path& operator=(Path const&) = default;
        Path& operator=(Path&&) noexcept = default;

        Path operator+(Path const& rhs) const;
        Path operator+(Vector2f delta) const;

        Path operator*(float scale) const;
        Path& operator+=(Vector2f delta);
        Path& operator*=(float scale);

        Path& operator+=(Path const& rhs);

        bool isEmpty() const;

        Region fillRegion(pmr::memory_resource* memory, FillRule rule,
                Vector2f pixelSize, size_t resPerPixel = 100) const;
        Region offsetRegion(pmr::memory_resource* memory, JoinType join,
                EndType end, float width, Vector2f pixelSize,
                size_t resPerPixel = 100) const;

        Rect getControlBb() const;
        Obb getControlObb() const;

        template <class THash>
        friend void hash_append(THash& h, Path const& path) noexcept
        {
            using btl::hash_append;
            if (path.deferred_)
            {
                hash_append(h, path.transform_);
                hash_append(h, path.getSegments());
                hash_append(h, path.getVertices());
            }
        }
    private:
        friend class PathBuilder;

        Path(std::vector<SegmentType>&& segments,
                std::vector<Vector2f>&& vertices);

        void ensureUniqueness();

        std::vector<SegmentType> const& getSegments() const;
        std::vector<Vector2f> const& getVertices() const;

        AVG_EXPORT friend Path operator*(const Transform& t, const Path& p);
        AVG_EXPORT friend std::ostream& operator<<(std::ostream&, const Path& p);
        std::shared_ptr<PathDeferred> deferred_;
        inline PathDeferred* d() { return deferred_.get(); }
        inline PathDeferred const* d() const { return deferred_.get(); }

        Transform transform_;
        Rect controlBb_;
    };

}

