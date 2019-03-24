#include "widget/widgetobject.h"
#include "widgetmaps.h"

namespace reactive::widget
{

static_assert(std::is_copy_constructible_v<WidgetObject>, "");
static_assert(std::is_nothrow_move_assignable_v<WidgetObject>, "");

namespace
{
    Widget makeWidgetFromFactory(WidgetFactory f, Signal<avg::Transform> t,
            Signal<avg::Vector2f> size)
    {
        auto f2 = f | transform(std::move(t));

        return std::move(f2)(std::move(size));
    }
} // anonymous namespace

WidgetObject::WidgetObject(WidgetFactory factory) :
    sizeHint_(factory.getSizeHint()),
    sizeInput_(signal::input(avg::Vector2f(100, 100))),
    transformInput_(signal::input(avg::Transform())),
    /*widget_(
            (std::move(factory)
            | transform(signal::share(std::move(transformInput_.signal))))
            (std::move(sizeInput_.signal))
            )
            */
    widget_(makeWidgetFromFactory(std::move(factory),
                std::move(transformInput_.signal),
                std::move(sizeInput_.signal)))
{
    auto nn = signal::share(std::move(transformInput_.signal));
    static_assert(std::is_copy_constructible<
            decltype(signal::share(std::move(transformInput_.signal)))>::value,"");
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

