#include "reactive/widget/withparams.h"
#include <reactive/widget/setparams.h>
#include <reactive/widget/setparamsobject.h>
#include <reactive/widget/modifyparamsobject.h>
#include <reactive/widget/withparamsobject.h>
#include <reactive/widget/margin.h>
#include <reactive/widget/frame.h>
#include <reactive/widget/instancemodifier.h>
#include <reactive/widget/elementmodifier.h>
#include <reactive/widget/widget.h>

#include <reactive/signal/signal.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

using namespace reactive;
using namespace reactive::widget;

auto makeEmptyInstance()
{
    return makeInstance(signal::constant(avg::Vector2f(0.0f, 0.0f)));
}

struct TestTag
{
    using type = std::string;
    static AnySharedSignal<type> getDefaultValue()
    {
        return share(signal::constant<std::string>("default value"));
    }
};

TEST(WidgetInstanceModifier, typeErasure)
{
    auto t = makeEmptyInstanceModifier();

    AnyInstanceModifier t2(std::move(t));

    auto w = makeEmptyInstance()
        | std::move(t2);

    static_assert(reactive::signal::IsSignalType<decltype(w), Instance>::value);
}

TEST(Widget, widgetBuildParameters)
{
    std::string tag1;
    std::string tag2;

    auto widget = makeWidget()
        | withParamsObject([&](auto widget, BuildParams const& params)
            {
                auto p = params.get<TestTag>();

                tag1 = p ? p->evaluate() : "no p";

                return widget;
            })
        | modifyParamsObject([](BuildParams params)
            {
                params.set<TestTag>(share(signal::constant<std::string>("set value 1")));
                return params;
            })
        | withParamsObject([&](auto widget, BuildParams const& params)
            {
                auto p = params.get<TestTag>();

                tag2 = p ? p->evaluate() : "no p";

                return widget;
            })
        | modifyParamsObject([](BuildParams params)
            {
                params.set<TestTag>(share(signal::constant<std::string>("set value 2")));
                return params;
            })
        ;

    BuildParams params;
    auto builder = std::move(widget)(std::move(params));

    std::move(builder)(signal::constant(avg::Vector2f(400.0f, 300.0f)));

    EXPECT_EQ("set value 1", tag1);
    EXPECT_EQ("set value 2", tag2);
}

TEST(Widget, differentModifiers)
{
    auto widget = makeWidget()
        | frame()
        | margin(signal::constant(10.0f))
        ;

    BuildParams params;
    auto builder = std::move(widget)(std::move(params));

    auto instanceSignal = std::move(builder)(
            signal::constant(avg::Vector2f(200.0f, 400.0f))
            ).getInstance();

    auto instance = instanceSignal.evaluate();

    EXPECT_EQ(avg::Vector2f(200.0f, 400.0f), instance.getSize());
}

TEST(Widget, withParams)
{
    std::string tag;

    auto widget = makeWidget()
        | withParams<TestTag>([&](auto widget, auto str)
            {
                tag = str.evaluate();
                return widget;
            })
        ;

    auto builder = std::move(widget)(BuildParams());

    std::move(builder)(signal::constant(avg::Vector2f(200.0f, 400.0f)));

    EXPECT_EQ("default value", tag);
}

TEST(Widget, setParams)
{
    std::string tag;

    auto widget = makeWidget()
        | withParams<TestTag>([&](auto widget, auto str)
            {
                tag = str.evaluate();
                return widget;
            })
        | setParams<TestTag>("set value")
        ;

    auto builder = std::move(widget)(BuildParams());

    std::move(builder)(signal::constant(avg::Vector2f(200.0f, 400.0f)));

    EXPECT_EQ("set value", tag);
}

TEST(Widget, builderModifierTags)
{
    std::string tag;
    std::string tag2;
    std::string tag3;

    auto widget = makeWidget()
        | makeBuilderModifier<TestTag>([&](auto builder, auto tagValue)
            {
                tag = tagValue.evaluate();
                return builder;
            })
        | setParams<TestTag>("set value 1")
        | makeBuilderModifier<TestTag>([&](auto builder, auto tagValue)
            {
                tag2 = tagValue.evaluate();
                return builder;
            })
        | setParams<TestTag>("set value 2")
        | makeBuilderModifier<TestTag>([&](auto builder, auto tagValue)
            {
                tag3 = tagValue.evaluate();
                return builder;
            })
        ;

    std::move(widget)(BuildParams());

    EXPECT_EQ("set value 1", tag);
    EXPECT_EQ("set value 2", tag2);
    EXPECT_EQ("default value", tag3);
}

TEST(Widget, elementModifierParams)
{
    std::string tag = "not set";
    std::string tag2 = "not set";
    std::string tag3 = "not set";

    auto widget = makeWidget()
        | makeElementModifier([&](auto element)
            {
                tag = element.getParams().template valueOrDefault<TestTag>().evaluate();
                return element;
            })
        | setParams<TestTag>("set value 1")
        | makeElementModifier([&](auto element)
            {
                tag2 = element.getParams().template valueOrDefault<TestTag>().evaluate();
                return element;
            })
        | setParams<TestTag>("set value 2")
        | makeElementModifier([&](auto element)
            {
                tag3 = element.getParams().template valueOrDefault<TestTag>().evaluate();
                return element;
            })
        ;

    std::move(widget)(BuildParams())(signal::constant(avg::Vector2f(100.0f, 200.0f)));

    EXPECT_EQ("set value 1", tag);
    EXPECT_EQ("set value 2", tag2);
    EXPECT_EQ("default value", tag3);
}

