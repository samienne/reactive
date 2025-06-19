#pragma once

#include "widget.h"

#include <bq/signal/signal.h>

#include <btl/shared.h>

namespace reactive::widget
{
    class REACTIVE_EXPORT WidgetObject
    {
    public:
        WidgetObject(AnyWidget widget, BuildParams const& params);
        WidgetObject(WidgetObject const&) = default;
        WidgetObject(WidgetObject&&) noexcept = default;

        WidgetObject& operator=(WidgetObject const&) = default;
        WidgetObject& operator=(WidgetObject&&) noexcept = default;

        void setObb(avg::Obb obb);
        void resize(avg::Vector2f size);
        void setTransform(avg::Transform t);

        signal::AnySignal<Instance> const& getWidget();
        avg::UniqueId const& getId() const;

        signal::AnySignal<SizeHint> const& getSizeHint() const;

    private:
        struct Impl
        {
            Impl(AnyWidget widget, BuildParams const& params);

            avg::UniqueId id_;
            signal::Input<signal::SignalResult<avg::Vector2f>,
                signal::SignalResult<avg::Vector2f>> sizeInput_;
            signal::Input<signal::SignalResult<avg::Transform>,
                signal::SignalResult<avg::Transform>> transformInput_;
            std::pair<
                signal::AnySignal<Instance>,
                btl::CloneOnCopy<signal::AnySignal<SizeHint>>
                > widget_;
        };

        btl::shared<Impl> impl_;
    };
} // namespace reactive::widget

