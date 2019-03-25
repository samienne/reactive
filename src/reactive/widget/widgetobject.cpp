#include "widget/widgetobject.h"
#include "widgetmaps.h"

namespace reactive::widget
{

static_assert(std::is_copy_constructible_v<WidgetObject>, "");
static_assert(std::is_nothrow_move_assignable_v<WidgetObject>, "");

WidgetObject::WidgetObject(WidgetFactory factory) :
    sizeHint_(factory.getSizeHint()),
    sizeInput_(signal::input(avg::Vector2f(100, 100))),
    transformInput_(signal::input(avg::Transform())),
    widget_(
            (std::move(factory)
            | transform(Signal<avg::Transform>(std::move(transformInput_.signal))))
            (std::move(sizeInput_.signal))
            )
{
}

void WidgetObject::setObb(avg::Obb obb)
{
    resize(obb.getSize());
    setTransform(obb.getTransform());
}

void WidgetObject::resize(avg::Vector2f size)
{
    sizeInput_.handle.set(size);
}

void WidgetObject::setTransform(avg::Transform t)
{
    transformInput_.handle.set(t);
}

Widget const& WidgetObject::getWidget() const
{
    return widget_;
}

Signal<SizeHint> const& WidgetObject::getSizeHint() const
{
    return *sizeHint_;
}

} // namespace reactive::widget

