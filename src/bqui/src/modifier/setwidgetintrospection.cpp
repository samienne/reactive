#include "bqui/modifier/setwidgetintrospection.h"

#include "bqui/modifier/buildermodifier.h"

#include "bqui/sizehint.h"

#include <algorithm>

namespace bqui::modifier
{

AnyWidgetModifier setWidgetIntrospection(
        bq::signal::AnySignal<widget::Introspection> introspection)
{
    return makeWidgetModifier(makeBuilderModifier(
        [](auto builder, auto introspection)
        {
            return std::move(builder).setIntrospection(std::move(introspection));
        },
        std::move(introspection)
        ));
}

AnyWidgetModifier setWidgetIntrospection(widget::Introspection introspection)
{
    return setWidgetIntrospection(
            bq::signal::constant(std::move(introspection)));
}

AnyWidgetModifier setName(bq::signal::AnySignal<std::string> name)
{
    return makeWidgetModifier(makeBuilderModifier(
        [](auto builder, auto name)
        {
            auto introspection = merge(builder.getIntrospection(),
                    std::move(name))
                .map([](widget::Introspection data, std::string name)
                {
                    data.name = std::move(name);
                    return data;
                });

            return std::move(builder).setIntrospection(std::move(introspection));
        },
        std::move(name)
        ));
}

AnyWidgetModifier setName(std::string name)
{
    return setName(bq::signal::constant(std::move(name)));
}

AnyWidgetModifier setRole(bq::signal::AnySignal<std::string> role)
{
    return makeWidgetModifier(makeBuilderModifier(
        [](auto builder, auto role)
        {
            auto introspection = merge(builder.getIntrospection(),
                    std::move(role))
                .map([](widget::Introspection data, std::string role)
                {
                    data.role = std::move(role);
                    return data;
                });

            return std::move(builder).setIntrospection(std::move(introspection));
        },
        std::move(role)
        ));
}

AnyWidgetModifier setRole(std::string role)
{
    return setRole(bq::signal::constant(std::move(role)));
}

AnyWidgetModifier setData(std::string key,
        bq::signal::AnySignal<widget::DataValue> value)
{
    return makeWidgetModifier(makeBuilderModifier(
        [](auto builder, std::string key, auto value)
        {
            auto introspection = merge(builder.getIntrospection(),
                    std::move(value))
                .map([key=std::move(key)](widget::Introspection data,
                        widget::DataValue value)
                {
                    data.data[key] = std::move(value);
                    return data;
                });

            return std::move(builder).setIntrospection(std::move(introspection));
        },
        std::move(key),
        std::move(value)
        ));
}

AnyWidgetModifier setData(std::string key, widget::DataValue value)
{
    return setData(std::move(key), bq::signal::constant(std::move(value)));
}

namespace
{
    // The size a hint asks for when unconstrained: its natural size (index 1 of
    // the min/natural/stretch triple) per axis. Introspection obbs are resolved
    // at this size because the realised size is not known where introspection
    // is observed.
    avg::Vector2f naturalSize(bqui::SizeHint const& hint)
    {
        auto width = hint.getWidth();
        auto height = hint.getHeightForWidth(width[1]);
        return avg::Vector2f(width[1], height[1]);
    }
} // namespace

AnyWidgetModifier setIntrospectionObb()
{
    return makeWidgetModifier(makeBuilderModifier(
        [](auto builder)
        {
            auto obb = builder.getSizeHint().map([](bqui::SizeHint const& hint)
                {
                    return avg::Obb(naturalSize(hint));
                });

            auto introspection = merge(builder.getIntrospection(), std::move(obb))
                .map([](widget::Introspection data, avg::Obb obb)
                {
                    data.obb = std::move(obb);
                    return data;
                });

            return std::move(builder).setIntrospection(std::move(introspection));
        }
        ));
}

AnyWidgetModifier addCapability(widget::Capability capability)
{
    return makeWidgetModifier(makeBuilderModifier(
        [](auto builder, widget::Capability capability)
        {
            auto introspection = builder.getIntrospection().map(
                [capability](widget::Introspection data)
                {
                    if (std::find(data.capabilities.begin(),
                                data.capabilities.end(), capability)
                            == data.capabilities.end())
                    {
                        data.capabilities.push_back(capability);
                    }

                    return data;
                });

            return std::move(builder).setIntrospection(std::move(introspection));
        },
        capability
        ));
}

}
