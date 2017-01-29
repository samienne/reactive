#pragma once

#include <ase/vector.h>

#include <vector>

namespace avg
{
    class PathSpec
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

        PathSpec start(ase::Vector2f v) &&;
        PathSpec lineTo(ase::Vector2f v) &&;
        PathSpec conicTo(ase::Vector2f v1, ase::Vector2f v2) &&;
        PathSpec cubicTo(ase::Vector2f v1, ase::Vector2f v2,
                ase::Vector2f v3) &&;
        PathSpec close() &&;

    private:
        friend class Path;
        std::vector<SegmentType> segments_;
        std::vector<ase::Vector2f> vertices_;
        ase::Vector2f start_;
    };
}

namespace btl
{
    template <> struct is_contiguously_hashable<avg::PathSpec::SegmentType>
        : std::true_type {};
}

