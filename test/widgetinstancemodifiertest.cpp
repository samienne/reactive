#include <reactive/widget/instancemodifier.h>

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

