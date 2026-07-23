#pragma once

#include "endtype.h"
#include "jointype.h"
#include "fillrule.h"
#include "transform.h"
#include "vector.h"
#include "rect.h"
#include "avgvisibility.h"
#include "polyline.h"

#include <btl/buffer.h>

#include <pmr/vector.h>
#include <pmr/memory_resource.h>

#include <memory>
#include <vector>
#include <cstddef>

namespace avg
{
    class Region;
    class PathDeferred;
    class Transform;

    /**
     * @brief A single transversal crossing of an infinite line against a path.
     *
     * The crossing is reported in the path's resolved space, with the path's
     * transform already applied. Tangential touches, where the line grazes a
     * curve or vertex without passing through it, are not reported, and a
     * segment lying exactly along the line is a measure-zero coincidence that
     * yields no crossing.
     */
    struct PathCrossing
    {
        /** @brief The intersection point in resolved path space. */
        Vector2f point;

        /**
         * @brief Signed distance of point from linePoint along lineDir.
         *
         * Positive values lie on the lineDir side of linePoint, so a caller can
         * treat the line as a ray by keeping crossings with lineParam greater
         * than zero.
         */
        float lineParam;

        /** @brief Parameter within the crossed segment, in the range [0, 1). */
        float segmentT;

        /** @brief Index of the crossed segment in the walked segment sequence. */
        std::size_t segmentIndex;

        /**
         * @brief Sign of the path tangent crossed with lineDir, either +1 or -1.
         *
         * Summing these over one-sided crossings gives the winding number; the
         * even-odd rule is the parity of the crossing count.
         */
        int windingSign;
    };

    class AVG_EXPORT Path final
    {
    public:
        enum class SegmentType
        {
            start,
            line,
            conic,
            cubic,
            arc
        };

        struct StartSegment
        {
            SegmentType type;
            Vector2f v;
        };

        struct LineSegment
        {
            SegmentType type;
            Vector2f v;
        };

        struct ConicSegment
        {
            SegmentType type;
            Vector2f v1;
            Vector2f v2;
        };

        struct CubicSegment
        {
            SegmentType type;
            Vector2f v1;
            Vector2f v2;
            Vector2f v3;
        };

        struct ArcSegment
        {
            SegmentType type;
            Vector2f center;
            float angle;
        };

        class ConstIterator
        {
        public:
            explicit inline ConstIterator(void const* ptr) :
                ptr_(ptr)
            {
            }

            inline bool operator==(ConstIterator const& rhs) const
            {
                return ptr_ == rhs.ptr_;
            }

            inline bool operator!=(ConstIterator const& rhs) const
            {
                return ptr_ != rhs.ptr_;
            }

            inline bool operator<(ConstIterator const& rhs) const
            {
                return ptr_ < rhs.ptr_;
            }

            inline bool operator>(ConstIterator const& rhs) const
            {
                return ptr_ > rhs.ptr_;
            }

            inline SegmentType getType() const
            {
                return get<SegmentType>();
            }

            template <typename T>
            T const& get() const
            {
                return *reinterpret_cast<T const*>(ptr_);
            }

            inline std::size_t getSegmentSize() const
            {
                switch (getType())
                {
                    case SegmentType::start:
                        return sizeof(StartSegment);
                    case SegmentType::line:
                        return sizeof(LineSegment);
                    case SegmentType::conic:
                        return sizeof(ConicSegment);
                    case SegmentType::cubic:
                        return sizeof(CubicSegment);
                    case SegmentType::arc:
                        return sizeof(ArcSegment);
                }

                return 1;
            }

            inline ConstIterator& operator++()
            {
                ptr_ = reinterpret_cast<char const*>(ptr_) + getSegmentSize();

                return *this;
            }

            inline ConstIterator operator++(int)
            {
                void const* old = ptr_;
                ptr_ = reinterpret_cast<char const*>(ptr_) + getSegmentSize();

                return ConstIterator(old);
            }

            inline StartSegment const& getStart() const
            {
                assert(getType() == SegmentType::start);
                return get<StartSegment>();
            }

            inline LineSegment const& getLine() const
            {
                assert(getType() == SegmentType::line);
                return get<LineSegment>();
            }

            inline ConicSegment const& getConic() const
            {
                assert(getType() == SegmentType::conic);
                return get<ConicSegment>();
            }

            inline CubicSegment const& getCubic() const
            {
                assert(getType() == SegmentType::cubic);
                return get<CubicSegment>();
            }

            inline ArcSegment const& getArc() const
            {
                assert(getType() == SegmentType::arc);
                return get<ArcSegment>();
            }

        private:
            void const* ptr_;
        };

        explicit Path(pmr::memory_resource* memory);
        Path(Path const&) = default;
        Path(Path&&) noexcept = default;
        ~Path();

        pmr::memory_resource* getResource() const;

        /** @brief Returns the transform that resolves the path into world space. */
        Transform const& getTransform() const;

        /**
         * @brief Tests whether a point lies inside the filled path.
         *
         * The point is given in the path's resolved space, the same space the
         * path's transform maps into. Each subpath is treated as implicitly
         * closed, and membership is decided from the crossings a ray cast from
         * the point makes with the outline. The fill rule chooses how those
         * crossings are read: FILL_EVENODD takes the parity of the count, while
         * FILL_NONZERO, FILL_POSITIVE and FILL_NEGATIVE test the signed winding
         * number for being non-zero, positive or negative.
         *
         * The ray is cast in a generic direction, so an edge running exactly
         * along it is a measure-zero case that does not perturb the count.
         */
        bool contains(Vector2f p, FillRule rule = FILL_EVENODD) const;

        /**
         * @brief Computes every crossing of an infinite line against the path.
         *
         * The line passes through linePoint with direction lineDir, which need
         * not be a unit vector, and both are in the path's resolved space. Each
         * segment is intersected analytically rather than by flattening: a line
         * gives a linear equation, a conic a quadratic, a cubic a cubic, and an
         * arc a line-circle intersection.
         *
         * Every subpath is treated as implicitly closed, matching fill
         * semantics, so an open subpath gains a closing edge from its last point
         * back to its start. A crossing at a vertex shared by two segments is
         * reported once, tangential touches are not reported, and a segment
         * lying exactly along the line is a measure-zero coincidence that yields
         * no crossing. A degenerate lineDir of zero length yields no crossings.
         */
        std::vector<PathCrossing> lineCrossings(Vector2f linePoint,
                Vector2f lineDir) const;

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

        ConstIterator begin() const;
        ConstIterator end() const;

        Region fillRegion(pmr::memory_resource* memory, FillRule rule,
                Vector2f pointSize) const;
        Region offsetRegion(pmr::memory_resource* memory, JoinType join,
                EndType end, float width, Vector2f pointSize) const;

        Rect getControlBb() const;
        Obb getControlObb() const;

        Path with_resource(pmr::memory_resource* memory) const;

        pmr::vector<PolyLine> toPolyLines(pmr::memory_resource* memory,
                Vector2f pointSize) const;

    private:
        friend class PathBuilder;

        Path(btl::Buffer&& buffer);

        void ensureUniqueness();

        AVG_EXPORT friend Path operator*(const Transform& t, const Path& p);
        AVG_EXPORT friend std::ostream& operator<<(std::ostream&, const Path& p);
        inline PathDeferred* d() { return deferred_.get(); }
        inline PathDeferred const* d() const { return deferred_.get(); }

    private:
        pmr::memory_resource* memory_;
        std::shared_ptr<PathDeferred> deferred_;
        Transform transform_;
        Rect controlBb_;
    };

}

