#include "bqui/widgetobject.h"

#include "bqui/modifier/setid.h"
#include "bqui/modifier/transform.h"
#include "bqui/modifier/buildermodifier.h"

#include "avg/transform.h"

#include "btl/uniqueid.h"

#include <pmr/new_delete_resource.h>

namespace bqui
{

static_assert(std::is_copy_constructible_v<WidgetObject>, "");
static_assert(std::is_nothrow_move_assignable_v<WidgetObject>, "");

namespace
{
    template <typename T, typename U>
    auto buildWidgetObject(widget::AnyWidget widget, BuildParams const& params,
            bq::signal::Signal<T, avg::Vector2f> size,
            bq::signal::Signal<U, avg::Transform> t,
            avg::UniqueId id)
    {
        auto builder = (std::move(widget)
            | modifier::setId(bq::signal::constant(id))
            | modifier::transform(std::move(t))
            )(params)
            ;

        auto sizeHint = builder.getSizeHint();

        return std::make_pair(
                std::move(builder)(std::move(size)).getInstance(),
                std::move(sizeHint)
                );
    }


} // anonymous namespace

WidgetObject::Impl::Impl(widget::AnyWidget widget, BuildParams const& params) :
    sizeInput_(bq::signal::makeInput(avg::Vector2f(100, 100))),
    transformInput_(bq::signal::makeInput(avg::Transform())),
    widget_(buildWidgetObject(
                std::move(widget),
                params,
                sizeInput_.signal.clone(),
                transformInput_.signal.clone(),
                id_
                ))
{
}

WidgetObject::WidgetObject(widget::AnyWidget widget, BuildParams const& params) :
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

bq::signal::AnySignal<widget::Instance> const& WidgetObject::getWidget()
{
    return impl_->widget_.first;
}

avg::UniqueId const& WidgetObject::getId() const
{
    return impl_->id_;
}

bq::signal::AnySignal<SizeHint> const& WidgetObject::getSizeHint() const
{
    return *impl_->widget_.second;
}

}

