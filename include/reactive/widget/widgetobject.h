#pragma once

#include <reactive/widgetfactory.h>

#include <reactive/signal/input.h>

namespace reactive::widget
{
    class REACTIVE_EXPORT WidgetObject
    {
    public:
        explicit WidgetObject(WidgetFactory factory);

        void setObb(avg::Obb obb);
        void resize(avg::Vector2f size);
        void setTransform(avg::Transform t);

        Widget const& getWidget() const;

        Signal<SizeHint> const& getSizeHint() const;

    private:
        btl::CloneOnCopy<Signal<SizeHint>> sizeHint_;
        signal::Input<avg::Vector2f> sizeInput_;
        signal::Input<avg::Transform> transformInput_;

        Widget widget_;
    };
} // namespace reactive::widget

