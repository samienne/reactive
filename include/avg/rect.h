#pragma once

#include "vector.h"

#include <ostream>

namespace avg
{
    class Rect
    {
    public:
        Rect();
        Rect(Rect const&) = default;
        Rect(Rect&&) noexcept = default;
        Rect(Vector2f bottomLeft, Vector2f size);
        ~Rect();

        Rect& operator=(Rect const&) = default;
        Rect& operator=(Rect&&) noexcept = default;

        Vector2f getBottomLeft() const;
        Vector2f getTopRight() const;
        Vector2f getSize() const;
        Vector2f getCenter() const;
        float getLeft() const;
        float getRight() const;
        float getTop() const;
        float getBottom() const;

        bool isEmpty() const;

        bool contains(Vector2f pos) const;

        friend std::ostream& operator<<(std::ostream& stream, Rect const& r)
        {
            return stream << "Rect{" << r.size_[0] << "x" << r.size_[1] << "}";
        }

    private:
        Vector2f bottomLeft_ = Vector2f(0.0f, 0.0f);
        Vector2f size_ = Vector2f(-0.0f, -0.0f);
    };
}

