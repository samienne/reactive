#pragma once

#include "vector.h"
#include "transform.h"

#include <btl/forcenoexcept.h>
#include <btl/visibility.h>

#include <ostream>

namespace avg
{
    class Obb;

    class BTL_VISIBLE Rect
    {
    public:
        Rect();
        Rect(Rect const&) = default;
        Rect(Rect&&) noexcept = default;
        Rect(Vector2f bottomLeft, Vector2f size);
        ~Rect();

        Rect& operator=(Rect const&) = default;
        Rect& operator=(Rect&&) noexcept = default;

        bool operator==(Rect const& rhs) const;

        Vector2f getBottomLeft() const;
        Vector2f getBottomRight() const;
        Vector2f getTopRight() const;
        Vector2f getTopLeft() const;
        Vector2f getSize() const;
        Vector2f getCenter() const;
        float getLeft() const;
        float getRight() const;
        float getTop() const;
        float getBottom() const;
        float getWidth() const;
        float getHeight() const;

        bool isEmpty() const;

        bool contains(Vector2f pos) const;

        bool overlaps(Rect const& r) const;
        bool isFullyContainedIn(Rect const& r) const;

        Rect scaled(float scale) const;
        Rect enlarged(float amount) const;

        friend std::ostream& operator<<(std::ostream& stream, Rect const& r)
        {
            return stream << "Rect{"
                << r.getLeft() << "," << r.getBottom() << " "
                << r.size_[0] << "x" << r.size_[1] << "}";
        }

        Rect include(Vector2f point) const;

    private:
        btl::ForceNoexcept<Vector2f> bottomLeft_ = Vector2f(0.0f, 0.0f);
        btl::ForceNoexcept<Vector2f> size_ = Vector2f(-0.0f, -0.0f);
    };
} // namespace avg

