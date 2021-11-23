#include <reactive/widget/widgettransformer.h>

#include <reactive/signal/signal.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

using namespace reactive;
using namespace reactive::widget;

auto makeEmptyWidget()
{
    return makeWidget(signal::constant(avg::Vector2f(0.0f, 0.0f)));
}

TEST(WidgetTransformer, simpleBind)
{
    auto t = makeWidgetTransformer()
        .bind([]()
        {
            return makeWidgetTransformer();
        });

    auto w = std::move(t)(makeEmptyWidget());

    static_assert(IsWidget<decltype(w.first)>::value);
    static_assert(std::is_same_v<std::tuple<>, std::decay_t<decltype(*w.second)>>);
}

TEST(WidgetTransformer, simpleCompose)
{
    auto t = makeWidgetTransformer()
        .compose(makeWidgetTransformer([](auto w)
            {
                auto obb = signal::share(w.getObb());
                auto w2 = std::move(w).setObb(obb);

                return provideValues(std::move(obb))(std::move(w2));
            }))
        ;

    auto w = std::move(t)(makeEmptyWidget());

    static_assert(IsWidget<decltype(w.first)>::value);
    static_assert(std::is_convertible_v<
            std::decay_t<decltype(*w.second)>,
            std::tuple<AnySignal<avg::Obb>>
            >);
}

TEST(WidgetTransformer, typeErasure)
{
    auto t = makeWidgetTransformer()
        .bind([]()
        {
            return makeWidgetTransformer();
        });

    WidgetTransformer<void> t2(std::move(t));

    auto w = std::move(t2)(makeEmptyWidget());

    static_assert(IsWidget<decltype(w.first)>::value);
    static_assert(std::is_same_v<std::tuple<>, std::decay_t<decltype(*w.second)>>);
}

