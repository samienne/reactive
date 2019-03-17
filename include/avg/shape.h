#pragma once

#include "pen.h"
#include "brush.h"
#include "path.h"
#include "avgvisibility.h"

#include <btl/option.h>
#include <btl/hash.h>

namespace avg
{
    class AVG_EXPORT Shape final
    {
    public:
        Shape();
        Shape(Shape const&) = default;
        Shape(Shape&&) noexcept = default;
        ~Shape();

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

        template <class THash>
        friend void hash_append(THash& h, Shape const& shape) noexcept
        {
            using btl::hash_append;
            hash_append(h, shape.path_);
            hash_append(h, shape.brush_);
            hash_append(h, shape.pen_);
        }

        AVG_EXPORT friend Shape operator*(Transform const& t, Shape const& rhs)
        {
            return Shape()
                .setPath(t * rhs.path_)
                .setBrush(t * rhs.brush_)
                .setPen(t * rhs.pen_);
        }

    private:
        Path path_;
        btl::option<Brush> brush_;
        btl::option<Pen> pen_;
    };
}

