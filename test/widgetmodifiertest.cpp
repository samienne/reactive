#include <reactive/widget/widgetmodifier.h>

#include <reactive/signal/signal.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

using namespace reactive;
using namespace reactive::widget;

auto makeEmptyWidget()
{
    return makeWidget(signal::constant(avg::Vector2f(0.0f, 0.0f)));
}

TEST(WidgetTransformer, typeErasure)
{
    auto t = makeEmptyWidgetModifier();

    AnyWidgetModifier t2(std::move(t));

    auto w = makeEmptyWidget()
        | std::move(t2);

    static_assert(reactive::signal::IsSignalType<decltype(w), Widget>::value);
}

