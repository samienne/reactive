#include "shapes.h"

#include <avg/pathspec.h>
#include <avg/path.h>

#include <ase/vector.h>

namespace reactive
{

float const pi = 3.14159f;

avg::Path makeRect(float width, float height)
{
    float w = width / 2.0f;
    float h = height / 2.0f;

    return avg::Path(avg::PathSpec()
            .start(ase::Vector2f(-w, -h))
            .lineTo(ase::Vector2f(w, -h))
            .lineTo(ase::Vector2f(w, h))
            .lineTo(ase::Vector2f(-w, h))
            .close()
            );
}

avg::Path makeRoundedRect(float width, float height, float radius)
{
    float w = width / 2.0f;
    float h = height / 2.0f;

    float const angle = pi / 2.0f;

    return avg::Path(avg::PathSpec()
            .start(ase::Vector2f(-w, -h+radius))
            .arc(ase::Vector2f(-w+radius,-h+radius), angle)
            .lineTo(ase::Vector2f(w - radius, -h))
            .arc(ase::Vector2f(w-radius, -h+radius), angle)
            .lineTo(ase::Vector2f(w, h-radius))
            .arc(ase::Vector2f(w-radius, h-radius), angle)
            .lineTo(ase::Vector2f(-w+radius, h))
            .arc(ase::Vector2f(-w+radius, h-radius), angle)
            .close()
            );
}

avg::Path makePathFromRect(avg::Rect const& rect, float radius)
{
    float const angle = 3.141 / 2.0f;

    float l = rect.getLeft();
    float r = rect.getRight();
    float t = rect.getTop();
    float b = rect.getBottom();

    if (radius < 0.0f)
        radius = 0.0f;
    if (radius > (rect.getWidth() / 2.0f))
        radius = rect.getWidth() / 2.0f;
    if (radius > (rect.getHeight() / 2.0f))
        radius = rect.getHeight() / 2.0f;

    if (radius < 0.0001f)
    {
        return avg::Path(avg::PathSpec()
                .start(ase::Vector2f(l, b))
                .lineTo(ase::Vector2f(r, b))
                .lineTo(ase::Vector2f(r, t))
                .lineTo(ase::Vector2f(l, t))
                .close()
                );
    }
    else
    {
        return avg::Path(avg::PathSpec()
                .start(ase::Vector2f(l, b+radius))
                .arc(ase::Vector2f(l+radius,b+radius), angle)
                .lineTo(ase::Vector2f(r - radius, b))
                .arc(ase::Vector2f(r-radius, b+radius), angle)
                .lineTo(ase::Vector2f(r, t-radius))
                .arc(ase::Vector2f(r-radius, t-radius), angle)
                .lineTo(ase::Vector2f(l+radius, t))
                .arc(ase::Vector2f(l+radius, t-radius), angle)
                .close()
                );
    }
}

REACTIVE_EXPORT avg::Path makeCircle(ase::Vector2f center, float radius)
{
    return avg::Path(avg::PathSpec()
            .start(center[0]+radius, center[1])
            .arc(center, 2.0f * pi)
            .close()
            );
}

avg::Shape makeShape(avg::Path const& path,
        btl::option<avg::Brush> const& brush,
        btl::option<avg::Pen> const& pen)
{
    return avg::Shape()
        .setPath(path)
        .setBrush(brush)
        .setPen(pen);
}

} // namespace reactive

