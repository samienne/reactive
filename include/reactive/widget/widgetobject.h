#pragma once

#include <reactive/widgetfactory.h>

#include <reactive/signal/input.h>

#include <btl/shared.h>

namespace reactive::widget
{
    class REACTIVE_EXPORT WidgetObject
    {
    public:
        WidgetObject(WidgetFactory factory);
        WidgetObject(WidgetObject const&) = default;
        WidgetObject(WidgetObject&&) noexcept = default;

        WidgetObject& operator=(WidgetObject const&) = default;
        WidgetObject& operator=(WidgetObject&&) noexcept = default;

        void setObb(avg::Obb obb);
        void resize(avg::Vector2f size);
        void setTransform(avg::Transform t);

        Widget const& getWidget() const;

        Signal<SizeHint> const& getSizeHint() const;

    private:
        struct Impl
        {
            Impl(WidgetFactory factory);

            btl::CloneOnCopy<Signal<SizeHint>> sizeHint_;
            signal::Input<DrawContext> drawContext_;
            signal::Input<avg::Vector2f> sizeInput_;
            signal::Input<avg::Transform> transformInput_;

            Widget widget_;
        };

        btl::shared<Impl> impl_;
    };
} // namespace reactive::widget

