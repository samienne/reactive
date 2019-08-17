#pragma once

#include "pen.h"
#include "brush.h"
#include "path.h"
#include "avgvisibility.h"

#include <btl/option.h>

#include <pmr/memory_resource.h>

namespace avg
{
    class AVG_EXPORT Shape final
    {
    public:
        explicit Shape(pmr::memory_resource* memory);
        Shape(Path path, btl::option<Brush> brush, btl::option<Pen> pen);
        Shape(Shape const&) = default;
        Shape(Shape&&) noexcept = default;
        ~Shape();

        pmr::memory_resource* getResource() const;

        Shape& operator=(Shape const&) = default;
        Shape& operator=(Shape&&) noexcept = default;

        Shape setPath(Path const& path) &&;
        Shape setBrush(btl::option<Brush> const& brush) &&;
        Shape setPen(btl::option<Pen> const& pen) &&;

        Path const& getPath() const;
        btl::option<Pen> const& getPen() const;
        btl::option<Brush> const& getBrush() const;
        Rect getControlBb() const;
        Obb getControlObb() const;

        Shape operator*(float scale) const &;
        Shape operator*(float scale) &&;
        Shape operator+(Vector2f offset) const &;
        Shape operator+(Vector2f offset) &&;

        Shape& operator*=(float scale);
        Shape& operator+=(Vector2f offset);

        bool operator==(Shape const& rhs) const;
        bool operator!=(Shape const& rhs) const;

        inline friend Shape operator*(Transform const& t, Shape const& rhs)
        {
            return Shape(t * rhs.path_, t * rhs.brush_, t * rhs.pen_);
        }

        Shape with_resource(pmr::memory_resource*) const;

    private:
        Path path_;
        btl::option<Brush> brush_;
        btl::option<Pen> pen_;
    };
}

