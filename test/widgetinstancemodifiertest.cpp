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
    struct TestTag
    {
        using type = std::string;
    };

    auto widget = makeWidget()
        | withParams([](auto widget, BuildParams const& params)
            {
                auto p = params.get<TestTag>();
                if (p)
                    std::cout << "2. " << p->evaluate() << std::endl;
                else
                    std::cout << "No p" << std::endl;

                return widget;
            })
        | modifyParams([](BuildParams params)
            {
                params.set<TestTag>(share(signal::constant<std::string>("test")));
                return params;
            })
        | withParams([](auto widget, BuildParams const& params)
            {
                auto p = params.get<TestTag>();
                if (p)
                    std::cout << "1. " << p->evaluate() << std::endl;
                else
                    std::cout << "No p" << std::endl;

                return widget;
            })
        | modifyParams([](BuildParams params)
            {
                params.set<TestTag>(share(signal::constant<std::string>("foo")));
                return params;
            })
        ;

    BuildParams params;
    auto builder = std::move(widget)(std::move(params));
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

