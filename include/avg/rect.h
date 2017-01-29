#pragma once

#include <ase/vector.h>

#include <ostream>

namespace avg
{
    class Rect
    {
    public:
        Rect();
        Rect(Rect const&) = default;
        Rect(Rect&&) = default;
        Rect(ase::Vector2f bottomLeft, ase::Vector2f size);
        ~Rect();

        Rect& operator=(Rect const&) = default;
        Rect& operator=(Rect&&) = default;

        ase::Vector2f getBottomLeft() const;
        ase::Vector2f getTopRight() const;
        ase::Vector2f getSize() const;
        ase::Vector2f getCenter() const;
        float getLeft() const;
        float getRight() const;
        float getTop() const;
        float getBottom() const;

        bool isEmpty() const;

        bool contains(ase::Vector2f pos) const;

        friend std::ostream& operator<<(std::ostream& stream, Rect const& r)
        {
            return stream << "Rect{" << r.size_[0] << "x" << r.size_[1] << "}";
        }

    private:
        ase::Vector2f bottomLeft_ = ase::Vector2f(0.0f, 0.0f);
        ase::Vector2f size_ = ase::Vector2f(-0.0f, -0.0f);
    };
}

