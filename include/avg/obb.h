#pragma once

#include "transform.h"
#include "avgvisibility.h"

#include <btl/forcenoexcept.h>

#include <ostream>

namespace avg
{
    class Rect;

    class AVG_EXPORT Obb
    {
    public:
        Obb();
        Obb(Obb const&) = default;
        Obb(Obb&&) noexcept = default;

        Obb& operator=(Obb const&) = default;
        Obb& operator=(Obb&&) noexcept = default;

        Obb(Vector2f size, Transform const& transform = Transform());
        Obb(Rect const& r);

        bool contains(Vector2f p) const;
        Vector2f getSize() const;
        Vector2f getCenter() const;
        Transform const& getTransform() const;

        Obb transformR(avg::Transform const& t) const;
        Obb setSize(Vector2f size) const;

        Rect getBoundingRect() const;

        //Obb operator+(Obb const& obb) const;

        bool operator==(Obb const& obb) const;
        bool operator!=(Obb const& obb) const;

        AVG_EXPORT friend Obb operator*(avg::Transform const& t,
                Obb const& obb);
        AVG_EXPORT friend std::ostream& operator<<(std::ostream& stream,
                Obb const& obb);

    private:
        Transform transform_;
        btl::ForceNoexcept<Vector2f> size_;
    };
}

