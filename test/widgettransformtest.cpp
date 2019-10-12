#include <reactive/widget/widgettransform.h>
#include <reactive/signal.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

using namespace reactive;
using namespace reactive::widget;

auto makeEmptyWidget()
{
    return makeWidget(
            signal::constant(DrawContext(pmr::new_delete_resource())),
            signal::constant(avg::Vector2f(0.0f, 0.0f))
            );
}

TEST(WidgetTransform, simpleBind)
{
    auto t = makeWidgetTransform()
        .bind([]()
        {
            return makeWidgetTransform();
        });

    auto w = std::move(t)(makeEmptyWidget());

    static_assert(IsWidget<decltype(w.first)>::value);
    static_assert(std::is_same_v<std::tuple<>, std::decay_t<decltype(*w.second)>>);
}

TEST(WidgetTransform, simpleProvide)
{
    auto t = makeWidgetTransform()
        .provide(makeWidgetTransform([](auto w)
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
            std::tuple<Signal<avg::Obb>>
            >);
}

TEST(WidgetTransform, typeErasure)
{
    auto t = makeWidgetTransform()
        .bind([]()
        {
            return makeWidgetTransform();
        });

    WidgetTransform<void> t2(std::move(t));

    auto w = std::move(t2)(makeEmptyWidget());

    static_assert(IsWidget<decltype(w.first)>::value);
    static_assert(std::is_same_v<std::tuple<>, std::decay_t<decltype(*w.second)>>);
}

