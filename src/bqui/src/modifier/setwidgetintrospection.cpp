#include "bqui/modifier/setwidgetintrospection.h"

#include "bqui/modifier/instancemodifier.h"

#include "bqui/widget/widget.h"

#include <algorithm>

namespace bqui::modifier
{

AnyWidgetModifier setWidgetIntrospection(
        bq::signal::AnySignal<widget::Introspection> introspection)
{
    return makeWidgetModifier(makeInstanceModifier(
        [](widget::Instance instance, widget::Introspection introspection)
        {
            // The obb is realised geometry owned by the instance, not authored
            // here; keep it so the escape hatch still reports real bounds.
            introspection.obb = instance.getIntrospection().obb;

            return std::move(instance)
                .setIntrospection(std::move(introspection));
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
    return makeWidgetModifier(makeInstanceModifier(
        [](widget::Instance instance, std::string name)
        {
            auto data = instance.getIntrospection();
            data.name = std::move(name);

            return std::move(instance).setIntrospection(std::move(data));
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
    return makeWidgetModifier(makeInstanceModifier(
        [](widget::Instance instance, std::string role)
        {
            auto data = instance.getIntrospection();
            data.role = std::move(role);

            return std::move(instance).setIntrospection(std::move(data));
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
    return makeWidgetModifier(makeInstanceModifier(
        [key=std::move(key)](widget::Instance instance,
                widget::DataValue value)
        {
            auto data = instance.getIntrospection();
            data.data[key] = std::move(value);

            return std::move(instance).setIntrospection(std::move(data));
        },
        std::move(value)
        ));
}

AnyWidgetModifier setData(std::string key, widget::DataValue value)
{
    return setData(std::move(key), bq::signal::constant(std::move(value)));
}

AnyWidgetModifier addCapability(widget::Capability capability)
{
    return makeWidgetModifier(makeInstanceModifier(
        [capability](widget::Instance instance)
        {
            auto data = instance.getIntrospection();
            if (std::find(data.capabilities.begin(), data.capabilities.end(),
                        capability) == data.capabilities.end())
            {
                data.capabilities.push_back(capability);
            }

            return std::move(instance).setIntrospection(std::move(data));
        }
        ));
}

}
