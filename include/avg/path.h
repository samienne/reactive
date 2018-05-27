#pragma once

#include "endtype.h"
#include "jointype.h"
#include "fillrule.h"
#include "transform.h"
#include "pathspec.h"
#include "vector.h"

#include <btl/hash.h>
#include <btl/visibility.h>

#include <vector>
#include <memory>

namespace avg
{
    class Region;
    class PathDeferred;
    class Transform;

    class BTL_VISIBLE Path final
    {
    public:
        using SegmentType = PathSpec::SegmentType;

        Path();
        Path(PathSpec&& pathSpec);
        Path(PathSpec const& pathSpec);
        Path(Path const&) = default;
        Path(Path&&) = default;
        ~Path();

        bool operator==(Path const& rhs) const;
        bool operator!=(Path const& rhs) const;

        Path& operator=(Path const&) = default;
        Path& operator=(Path&&) = default;

        Path operator+(Path const& rhs) const;
        Path operator+(Vector2f delta) const;

        Path operator*(float scale) const;
        Path& operator+=(Vector2f delta);
        Path& operator*=(float scale);

        Path& operator+=(Path const& rhs);

        bool isEmpty() const;

        Region fillRegion(FillRule rule, Vector2f pixelSize,
                size_t resPerPixel = 100) const;
        Region offsetRegion(JoinType join, EndType end, float width,
                Vector2f pixelSize, size_t resPerPixel = 100) const;

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
        Path(std::vector<SegmentType>&& segments,
                std::vector<Vector2f>&& vertices);

        void ensureUniqueness();

        std::vector<SegmentType> const& getSegments() const;
        std::vector<Vector2f> const& getVertices() const;

        BTL_VISIBLE friend Path operator*(const Transform& t, const Path& p);
        BTL_VISIBLE friend std::ostream& operator<<(std::ostream&, const Path& p);
        std::shared_ptr<PathDeferred> deferred_;
        inline PathDeferred* d() { return deferred_.get(); }
        inline PathDeferred const* d() const { return deferred_.get(); }

        Transform transform_;
    };

}

