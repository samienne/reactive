#include "widget/widgetobject.h"

#include "avg/transform.h"
#include "btl/uniqueid.h"
#include "widget/setid.h"
#include "widget/transform.h"
#include "widget/buildermodifier.h"

#include <pmr/new_delete_resource.h>

namespace reactive::widget
{

static_assert(std::is_copy_constructible_v<WidgetObject>, "");
static_assert(std::is_nothrow_move_assignable_v<WidgetObject>, "");

namespace
{
    template <typename T, typename U>
    auto buildWidgetObject(AnyWidget widget, BuildParams const& params,
            signal2::Signal<T, avg::Vector2f> size,
            signal2::Signal<U, avg::Transform> t,
            avg::UniqueId id)
    {
        auto builder = (std::move(widget)
            | widget::setId(signal2::constant(id))
            | transform(std::move(t))
            )(params)
            ;

        auto sizeHint = builder.getSizeHint();

        return std::make_pair(
                std::move(builder)(std::move(size)).getInstance(),
                std::move(sizeHint)
                );
    }


} // anonymous namespace

WidgetObject::Impl::Impl(AnyWidget widget, BuildParams const& params) :
    sizeInput_(signal2::makeInput(avg::Vector2f(100, 100))),
    transformInput_(signal2::makeInput(avg::Transform())),
    widget_(buildWidgetObject(
                std::move(widget),
                params,
                sizeInput_.signal.clone(),
                transformInput_.signal.clone(),
                id_
                ))
{
}

WidgetObject::WidgetObject(AnyWidget widget, BuildParams const& params) :
    impl_(std::make_shared<Impl>(std::move(widget), params))
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

signal2::AnySignal<Instance> const& WidgetObject::getWidget()
{
    return impl_->widget_.first;
}

avg::UniqueId const& WidgetObject::getId() const
{
    return impl_->id_;
}

signal2::AnySignal<SizeHint> const& WidgetObject::getSizeHint() const
{
    return *impl_->widget_.second;
}

} // namespace reactive::widget

