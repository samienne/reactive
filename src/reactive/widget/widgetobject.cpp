#include "widget/widgetobject.h"
#include "widget/transform.h"

#include <pmr/new_delete_resource.h>

namespace reactive::widget
{

static_assert(std::is_copy_constructible_v<WidgetObject>, "");
static_assert(std::is_nothrow_move_assignable_v<WidgetObject>, "");

WidgetObject::Impl::Impl(WidgetFactory factory, DrawContext drawContext) :
    sizeHint_(factory.getSizeHint()),
    drawContext_(signal::input(std::move(drawContext))),
    sizeInput_(signal::input(avg::Vector2f(100, 100))),
    transformInput_(signal::input(avg::Transform())),
    widget_((factory
            | transform(transformInput_.signal.clone()))
            (
             dropRepeats(drawContext_.signal),
             sizeInput_.signal
             )
           )
{
}

WidgetObject::WidgetObject(WidgetFactory factory) :
    factory_(factory)
{
}

void WidgetObject::setDrawContext(DrawContext drawContext)
{
    if (!impl_)
        impl_ = std::make_shared<Impl>(std::move(factory_), std::move(drawContext));
    else
        impl_->drawContext_.handle.set(std::move(drawContext));
}

void WidgetObject::setObb(avg::Obb obb)
{
    resize(obb.getSize());
    setTransform(obb.getTransform());
}

void WidgetObject::resize(avg::Vector2f size)
{
    assert(impl_);
    impl_->sizeInput_.handle.set(size);
}

void WidgetObject::setTransform(avg::Transform t)
{
    assert(impl_);
    impl_->transformInput_.handle.set(t);
}

Widget const& WidgetObject::getWidget() const
{
    assert(impl_);
    return impl_->widget_;
}

AnySignal<SizeHint> WidgetObject::getSizeHint() const
{
    if (impl_)
        return impl_->sizeHint_->clone();
    else
        return factory_.getSizeHint();
}

} // namespace reactive::widget

