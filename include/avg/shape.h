#pragma once

#include "pen.h"
#include "brush.h"
#include "path.h"
#include "avgvisibility.h"

#include <pmr/memory_resource.h>

#include <optional>

namespace avg
{
    class AVG_EXPORT Shape final
    {
    public:
        explicit Shape(pmr::memory_resource* memory);
        Shape(Path path, std::optional<Brush> brush, std::optional<Pen> pen);
        Shape(Shape const&) = default;
        Shape(Shape&&) noexcept = default;
        ~Shape();

        pmr::memory_resource* getResource() const;

        Shape& operator=(Shape const&) = default;
        Shape& operator=(Shape&&) noexcept = default;

        Shape setPath(Path const& path) &&;
        Shape setBrush(std::optional<Brush> const& brush) &&;
        Shape setPen(std::optional<Pen> const& pen) &&;

        Path const& getPath() const;
        std::optional<Pen> const& getPen() const;
        std::optional<Brush> const& getBrush() const;
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
        std::optional<Brush> brush_;
        std::optional<Pen> pen_;
    };
}

