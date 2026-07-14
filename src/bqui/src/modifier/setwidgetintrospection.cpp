#include "bqui/modifier/setwidgetintrospection.h"

#include "bqui/modifier/buildermodifier.h"

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

AnyWidgetModifier setName(std::string name)
{
    return makeWidgetModifier(makeBuilderModifier(
        [](auto builder, std::string name)
        {
            auto introspection = builder.getIntrospection().map(
                [name=std::move(name)](widget::Introspection data)
                {
                    data.name = name;
                    return data;
                });

            return std::move(builder).setIntrospection(std::move(introspection));
        },
        std::move(name)
        ));
}

AnyWidgetModifier setRole(std::string role)
{
    return makeWidgetModifier(makeBuilderModifier(
        [](auto builder, std::string role)
        {
            auto introspection = builder.getIntrospection().map(
                [role=std::move(role)](widget::Introspection data)
                {
                    data.role = role;
                    return data;
                });

            return std::move(builder).setIntrospection(std::move(introspection));
        },
        std::move(role)
        ));
}

}
