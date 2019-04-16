#pragma once

#include "vector.h"
#include "avgvisibility.h"

#include <vector>

namespace avg
{
    class AVG_EXPORT PathSpec
    {
    public:
        enum SegmentType
        {
            SEGMENT_START = 0,
            SEGMENT_LINE,
            SEGMENT_CONIC,
            SEGMENT_CUBIC
        };

        PathSpec();
        PathSpec(PathSpec const&) = default;
        PathSpec(PathSpec&&) = default;
        ~PathSpec();

        PathSpec& operator=(PathSpec const&) = default;
        PathSpec& operator=(PathSpec&&) = default;

        PathSpec start(Vector2f v) &&;
        PathSpec start(float x, float y) &&;
        PathSpec lineTo(Vector2f v) &&;
        PathSpec lineTo(float x, float y) &&;
        PathSpec conicTo(Vector2f v1, Vector2f v2) &&;
        PathSpec cubicTo(Vector2f v1, Vector2f v2, Vector2f v3) &&;
        PathSpec close() &&;

    private:
        friend class Path;
        std::vector<SegmentType> segments_;
        std::vector<Vector2f> vertices_;
        Vector2f start_;
    };
}

namespace btl
{
    template <> struct is_contiguously_hashable<avg::PathSpec::SegmentType>
        : std::true_type {};
}

