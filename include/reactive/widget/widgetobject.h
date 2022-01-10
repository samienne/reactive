#pragma once

#include "builder.h"

#include <reactive/signal/input.h>

#include <btl/shared.h>

#include <optional>

namespace reactive::widget
{
    class REACTIVE_EXPORT WidgetObject
    {
    public:
        WidgetObject(Builder builder);
        WidgetObject(WidgetObject const&) = default;
        WidgetObject(WidgetObject&&) noexcept = default;

        WidgetObject& operator=(WidgetObject const&) = default;
        WidgetObject& operator=(WidgetObject&&) noexcept = default;

        void setObb(avg::Obb obb);
        void resize(avg::Vector2f size);
        void setTransform(avg::Transform t);

        AnySignal<Instance> const& getWidget();
        avg::UniqueId const& getId() const;

        AnySignal<SizeHint> const& getSizeHint() const;

    private:
        struct Impl
        {
            Impl(Builder builder);

            avg::UniqueId id_;
            Builder builder_;
            btl::CloneOnCopy<AnySignal<SizeHint>> sizeHint_;
            signal::Input<avg::Vector2f> sizeInput_;
            signal::Input<avg::Transform> transformInput_;
            AnySignal<Instance> widget_;
        };

        btl::shared<Impl> impl_;
    };
} // namespace reactive::widget

