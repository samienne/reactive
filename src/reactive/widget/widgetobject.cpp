#include "widget/widgetobject.h"
#include "widget/transform.h"

#include <pmr/new_delete_resource.h>

namespace reactive::widget
{

static_assert(std::is_copy_constructible_v<WidgetObject>, "");
static_assert(std::is_nothrow_move_assignable_v<WidgetObject>, "");

WidgetObject::Impl::Impl(WidgetFactory factory) :
    sizeHint_(factory.getSizeHint()),
    drawContext_(signal::input(DrawContext(pmr::new_delete_resource()))),
    sizeInput_(signal::input(avg::Vector2f(100, 100))),
    transformInput_(signal::input(avg::Transform())),
    widget_(
            (std::move(factory)
            | transform(Signal<avg::Transform>(std::move(transformInput_.signal))))
            (
             std::move(drawContext_.signal),
             std::move(sizeInput_.signal)
             ))
{
}

WidgetObject::WidgetObject(WidgetFactory factory) :
    impl_(std::make_shared<Impl>(std::move(factory)))
{
}

void WidgetObject::setDrawContext(DrawContext drawContext)
{
    impl_->drawContext_.handle.set(std::move(drawContext));
}

void WidgetObject::setObb(avg::Obb obb)
{
    resize(obb.getSize());
    setTransform(obb.getTransform());
}

void WidgetObject::resize(avg::Vector2f size)
{
    impl_->sizeInput_.handle.set(size);
}

void WidgetObject::setTransform(avg::Transform t)
{
    impl_->transformInput_.handle.set(t);
}

Widget const& WidgetObject::getWidget() const
{
    return impl_->widget_;
}

Signal<SizeHint> const& WidgetObject::getSizeHint() const
{
    return *impl_->sizeHint_;
}

} // namespace reactive::widget

