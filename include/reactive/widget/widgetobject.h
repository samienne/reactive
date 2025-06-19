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

        bq::signal::AnySignal<Instance> const& getWidget();
        avg::UniqueId const& getId() const;

        bq::signal::AnySignal<SizeHint> const& getSizeHint() const;

    private:
        struct Impl
        {
            Impl(AnyWidget widget, BuildParams const& params);

            avg::UniqueId id_;
            bq::signal::Input<bq::signal::SignalResult<avg::Vector2f>,
                bq::signal::SignalResult<avg::Vector2f>> sizeInput_;
            bq::signal::Input<bq::signal::SignalResult<avg::Transform>,
                bq::signal::SignalResult<avg::Transform>> transformInput_;
            std::pair<
                bq::signal::AnySignal<Instance>,
                btl::CloneOnCopy<bq::signal::AnySignal<SizeHint>>
                > widget_;
        };

        btl::shared<Impl> impl_;
    };
} // namespace reactive::widget

