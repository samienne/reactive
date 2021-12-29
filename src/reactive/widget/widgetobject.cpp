#include "widget/widgetobject.h"

#include "widget/setid.h"
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
    transformInput_(signal::input(avg::Transform())),
    widget_((factory_
            | widget::setId(signal::constant(id_))
            | transform(transformInput_.signal.clone())
            )
            (sizeInput_.signal.clone())
            )
{
}

WidgetObject::WidgetObject(WidgetFactory factory) :
    impl_(std::make_shared<Impl>(std::move(factory)))
{
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

AnySignal<Widget> const& WidgetObject::getWidget()
{
    return impl_->widget_;
}

avg::UniqueId const& WidgetObject::getId() const
{
    return impl_->id_;
}

AnySignal<SizeHint> const& WidgetObject::getSizeHint() const
{
    return *impl_->sizeHint_;
}

} // namespace reactive::widget

