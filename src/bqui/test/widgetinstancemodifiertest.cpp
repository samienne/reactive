#include <bqui/modifier/setparams.h>
#include <bqui/modifier/setparamsobject.h>
#include <bqui/modifier/modifyparamsobject.h>
#include <bqui/modifier/margin.h>
#include <bqui/modifier/frame.h>
#include <bqui/modifier/instancemodifier.h>
#include <bqui/modifier/elementmodifier.h>

#include <bqui/provider/provideparam.h>

#include <bqui/widget/widget.h>

#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

using namespace bq;
using namespace bqui;
using namespace bqui::widget;
using namespace bqui::modifier;
using namespace bqui::provider;

auto makeEmptyInstance()
{
    return makeInstance(bq::signal::constant(avg::Vector2f(0.0f, 0.0f)));
}

struct TestTag
{
    using type = std::string;
    static signal::AnySignal<type> getDefaultValue()
    {
        return signal::constant<std::string>("default value");
    }
};

TEST(WidgetInstanceModifier, typeErasure)
{
    auto t = makeEmptyInstanceModifier();

    AnyInstanceModifier t2(std::move(t));

    auto w = makeEmptyInstance()
        | std::move(t2);

    //static_assert(bq::signal::IsSignalType<decltype(w), Instance>::value);
}

TEST(Widget, widgetBuildParameters)
{
    std::string tag1;
    std::string tag2;

    auto widget = makeWidget()
        | makeWidgetModifier([&](auto widget, BuildParams const& params)
            {
                auto p = params.get<TestTag>();

                tag1 = p ? bq::signal::makeSignalContext(*p).evaluate() : "no p";

                return widget;
            },
            provideBuildParams()
            )
        | modifyParamsObject([](BuildParams params)
            {
                params.set<TestTag>(signal::constant<std::string>("set value 1"));
                return params;
            })
        | makeWidgetModifier([&](auto widget, BuildParams const& params)
            {
                auto p = params.get<TestTag>();

                tag2 = p ? bq::signal::makeSignalContext(*p).evaluate() : "no p";

                return widget;
            },
            provideBuildParams()
            )
        | modifyParamsObject([](BuildParams params)
            {
                params.set<TestTag>(signal::constant<std::string>("set value 2"));
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

    auto context = makeSignalContext(instanceSignal);
    auto instance = context.evaluate();

    EXPECT_EQ(avg::Vector2f(200.0f, 400.0f), instance.getSize());
}

TEST(Widget, withParams)
{
    std::string tag;

    auto widget = makeWidget()
        | makeWidgetModifier([&](auto widget, auto str)
            {
                tag = signal::makeSignalContext(str).evaluate();
                return widget;
            }, provideParam<TestTag>())
        ;

    auto builder = std::move(widget)(BuildParams());

    std::move(builder)(signal::constant(avg::Vector2f(200.0f, 400.0f)));

    EXPECT_EQ("default value", tag);
}

TEST(Widget, setParams)
{
    std::string tag;

    auto widget = makeWidget()
        | makeWidgetModifier([&](auto widget, auto str)
            {
                tag = signal::makeSignalContext(str).evaluate();
                return widget;
            },
            provideParam<TestTag>()
            )
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
        | makeBuilderModifier([&](auto builder, auto tagValue)
            {
                tag = signal::makeSignalContext(tagValue).evaluate();
                return builder;
            }, provideParam<TestTag>())
        | setParams<TestTag>("set value 1")
        | makeBuilderModifier([&](auto builder, auto tagValue)
            {
                tag2 = signal::makeSignalContext(tagValue).evaluate();
                return builder;
            }, provideParam<TestTag>())
        | setParams<TestTag>("set value 2")
        | makeBuilderModifier([&](auto builder, auto tagValue)
            {
                tag3 = signal::makeSignalContext(tagValue).evaluate();
                return builder;
            }, provideParam<TestTag>())
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
                tag = signal::makeSignalContext(
                        element.getParams().template valueOrDefault<TestTag>()
                        ).evaluate();
                return element;
            })
        | setParams<TestTag>("set value 1")
        | makeElementModifier([&](auto element)
            {
                tag2 = signal::makeSignalContext(
                        element.getParams().template valueOrDefault<TestTag>()
                        ).evaluate();
                return element;
            })
        | setParams<TestTag>("set value 2")
        | makeElementModifier([&](auto element)
            {
                tag3 = signal::makeSignalContext(
                        element.getParams().template valueOrDefault<TestTag>()
                        ).evaluate();
                return element;
            })
        ;

    std::move(widget)(BuildParams())(signal::constant(avg::Vector2f(100.0f, 200.0f)));

    EXPECT_EQ("set value 1", tag);
    EXPECT_EQ("set value 2", tag2);
    EXPECT_EQ("default value", tag3);
}
