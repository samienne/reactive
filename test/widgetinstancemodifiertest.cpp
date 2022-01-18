#include "reactive/widget/withparams.h"
#include <reactive/widget/setparams.h>
#include <reactive/widget/setparamsobject.h>
#include <reactive/widget/modifyparamsobject.h>
#include <reactive/widget/withparamsobject.h>
#include <reactive/widget/instancemodifier.h>
#include <reactive/widget/margin.h>
#include <reactive/widget/frame.h>
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
    static AnySharedSignal<type> const defaultValue;
};

AnySharedSignal<std::string> const TestTag::defaultValue =
    share(signal::constant<std::string>("test"));

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
    auto widget = makeWidget()
        | withParamsObject([](auto widget, BuildParams const& params)
            {
                auto p = params.get<TestTag>();
                if (p)
                    std::cout << "2. " << p->evaluate() << std::endl;
                else
                    std::cout << "No p" << std::endl;

                return widget;
            })
        | modifyParamsObject([](BuildParams params)
            {
                params.set<TestTag>(share(signal::constant<std::string>("test")));
                return params;
            })
        | withParamsObject([](auto widget, BuildParams const& params)
            {
                auto p = params.get<TestTag>();
                if (p)
                    std::cout << "1. " << p->evaluate() << std::endl;
                else
                    std::cout << "No p" << std::endl;

                return widget;
            })
        | modifyParamsObject([](BuildParams params)
            {
                params.set<TestTag>(share(signal::constant<std::string>("foo")));
                return params;
            })
        ;

    BuildParams params;
    auto builder = std::move(widget)(std::move(params));

    std::move(builder)(signal::constant(avg::Vector2f(400.0f, 300.0f)));
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
            );

    auto instance = instanceSignal.evaluate();

    EXPECT_EQ(avg::Vector2f(200.0f, 400.0f), instance.getSize());
}

TEST(Widget, withParams)
{
    auto widget = makeWidget()
        | withParams<TestTag>([](auto widget, auto str)
            {
                std::cout << "Value: " << str.evaluate() << std::endl;
                return widget;
            })
        ;

    auto builder = std::move(widget)(BuildParams());

    std::move(builder)(signal::constant(avg::Vector2f(200.0f, 400.0f)));
}

TEST(Widget, setParams)
{
    auto widget = makeWidget()
        | withParams<TestTag>([](auto widget, auto str)
            {
                std::cout << "Value: " << str.evaluate() << std::endl;
                return widget;
            })
        | setParams<TestTag>(share(signal::constant<std::string>("test")))
        ;

    auto builder = std::move(widget)(BuildParams());

    std::move(builder)(signal::constant(avg::Vector2f(200.0f, 400.0f)));
}

