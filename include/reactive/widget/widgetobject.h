#pragma once

#include "widget.h"

#include <reactive/signal2/signal.h>

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

        signal2::AnySignal<Instance> const& getWidget();
        avg::UniqueId const& getId() const;

        signal2::AnySignal<SizeHint> const& getSizeHint() const;

    private:
        struct Impl
        {
            Impl(AnyWidget widget, BuildParams const& params);

            avg::UniqueId id_;
            signal2::Input<signal2::SignalResult<avg::Vector2f>,
                signal2::SignalResult<avg::Vector2f>> sizeInput_;
            signal2::Input<signal2::SignalResult<avg::Transform>,
                signal2::SignalResult<avg::Transform>> transformInput_;
            std::pair<
                signal2::AnySignal<Instance>,
                btl::CloneOnCopy<signal2::AnySignal<SizeHint>>
                > widget_;
        };

        btl::shared<Impl> impl_;
    };
} // namespace reactive::widget

