#pragma once

#include "transform.h"

#include <ostream>

namespace avg
{
    class Rect;

    class Obb
    {
    public:
        Obb();
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

        friend Obb operator*(avg::Transform const& t, Obb const& obb);
        friend std::ostream& operator<<(std::ostream& stream, Obb const& obb);

    private:
        Transform transform_;
        Vector2f size_;
    };
}

