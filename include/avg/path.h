#pragma once

#include "endtype.h"
#include "jointype.h"
#include "fillrule.h"
#include "transform.h"
#include "vector.h"
#include "rect.h"
#include "avgvisibility.h"

#include <btl/buffer.h>

#include <pmr/vector.h>
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
                Vector2f pixelSize, float resPerPixel = 4.0f) const;
        Region offsetRegion(pmr::memory_resource* memory, JoinType join,
                EndType end, float width, Vector2f pixelSize,
                float resPerPixel = 4.0f) const;

        Rect getControlBb() const;
        Obb getControlObb() const;

        Path with_resource(pmr::memory_resource* memory) const;

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

