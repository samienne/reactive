#pragma once

#include "vector.h"
#include "rect.h"
#include "path.h"
#include "avgvisibility.h"

#include <pmr/vector.h>
#include <pmr/memory_resource.h>

#include <vector>

namespace avg
{
    class AVG_EXPORT PathBuilder
    {
    public:
        using SegmentType = Path::SegmentType;

        PathBuilder(pmr::memory_resource* memory);
        PathBuilder(PathBuilder const&) = default;
        PathBuilder(PathBuilder&&) = default;
        ~PathBuilder();

        pmr::memory_resource* getResource() const;

        PathBuilder& operator=(PathBuilder const&) = default;
        PathBuilder& operator=(PathBuilder&&) = default;

        PathBuilder start(Vector2f v) &&;
        PathBuilder start(float x, float y) &&;
        PathBuilder lineTo(Vector2f v) &&;
        PathBuilder lineTo(float x, float y) &&;
        PathBuilder conicTo(Vector2f v1, Vector2f v2) &&;
        PathBuilder cubicTo(Vector2f v1, Vector2f v2, Vector2f v3) &&;
        PathBuilder arc(Vector2f center, float angle) &&;
        PathBuilder close() &&;

        Path build() &&;
        Path build() const &;

    private:
        friend class Path;
        pmr::vector<SegmentType> segments_;
        pmr::vector<Vector2f> vertices_;
        Vector2f start_;
    };
}

namespace btl
{
    template <> struct is_contiguously_hashable<avg::PathBuilder::SegmentType>
        : std::true_type {};
}

