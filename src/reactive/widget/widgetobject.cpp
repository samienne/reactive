#include "widget/widgetobject.h"
#include "widget/transform.h"

#include <pmr/new_delete_resource.h>

namespace reactive::widget
{

static_assert(std::is_copy_constructible_v<WidgetObject>, "");
static_assert(std::is_nothrow_move_assignable_v<WidgetObject>, "");

WidgetObject::Impl::Impl(WidgetFactory factory) :
    factory_(std::move(factory)),
    sizeHint_(factory_.getSizeHint()),
    sizeInput_(signal::input(avg::Vector2f(100, 100))),
    transformInput_(signal::input(avg::Transform()))
{
}

WidgetObject::WidgetObject(WidgetFactory factory) :
    impl_(std::make_shared<Impl>(std::move(factory)))
{
}

void WidgetObject::setDrawContext(DrawContext drawContext)
{
    if (impl_->drawContext_)
        impl_->drawContext_->handle.set((drawContext));
    else
        impl_->drawContext_ = signal::input((drawContext));


    if (!impl_->widget_)
    {
        assert(impl_->drawContext_);
        impl_->widget_ = (impl_->factory_
                    | transform(impl_->transformInput_.signal.clone()))
                (
                 dropRepeats(impl_->drawContext_->signal),
                 impl_->sizeInput_.signal
                )
                ;
    }
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

Widget const& WidgetObject::getWidget(DrawContext drawContext)
{
    setDrawContext(std::move(drawContext));

    assert(impl_->widget_);
    return *impl_->widget_;
}

AnySignal<SizeHint> const& WidgetObject::getSizeHint() const
{
    return *impl_->sizeHint_;
}

} // namespace reactive::widget

