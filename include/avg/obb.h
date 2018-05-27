#pragma once

#include "transform.h"

#include <btl/forcenoexcept.h>
#include <btl/visibility.h>

#include <ostream>

namespace avg
{
    class Rect;

    class BTL_VISIBLE Obb
    {
    public:
        Obb();
        Obb(Obb const&) = default;
        Obb(Obb&&) noexcept = default;

        Obb& operator=(Obb const&) = default;
        Obb& operator=(Obb&&) noexcept = default;

        Obb(Vector2f size);
        Obb(Rect const& r);

        bool contains(Vector2f p) const;
        Vector2f getSize() const;
        Vector2f getCenter() const;
        Transform const& getTransform() const;

        Obb transformR(avg::Transform const& t) const;
        Obb setSize(Vector2f size) const;

        Obb operator+(Obb const& obb) const;

        bool operator==(Obb const& obb) const;
        bool operator!=(Obb const& obb) const;

        BTL_VISIBLE friend Obb operator*(avg::Transform const& t, Obb const& obb);
        BTL_VISIBLE friend std::ostream& operator<<(std::ostream& stream, Obb const& obb);

    private:
        Transform transform_;
        btl::ForceNoexcept<Vector2f> size_;
    };
}

