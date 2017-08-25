#include <reactive/widgetmap.h>
#include <reactive/widgetgetters.h>
#include <reactive/widgetsetters.h>

#include <reactive/signal/map.h>
#include <reactive/signal/constant.h>
#include <reactive/signaltype.h>

#include <btl/fn.h>

#include <gtest/gtest.h>

using namespace reactive;

TEST(Widget, get)
{
    auto w =
        reactive::makeWidget(signal::constant(ase::Vector2f(100.0f, 100.0f)));

    auto d = reactive::get<avg::Drawing>(w);

    static_assert(std::is_same<
                signal2::Signal<avg::Drawing const&, signal::Constant<avg::Drawing>>,
                decltype(d)
            >::value,
            "");

    auto a = reactive::get<std::vector<InputArea>>(w);

    static_assert(std::is_same<
                signal2::Signal<std::vector<InputArea> const&,
                    signal::Constant<std::vector<InputArea>>
                >,
                decltype(a)
            >::value,
            "");

    auto o = reactive::get<avg::Obb>(w);

    static_assert(IsSignalType<
            decltype(o),
            avg::Obb
            >::value, "");

    auto s = reactive::get<ase::Vector2f>(w);

    static_assert(std::is_assignable<
                Signal<ase::Vector2f>,
                decltype(s)
            >::value,
            "");

    auto k = reactive::get<std::vector<KeyboardInput>>(w);

    static_assert(std::is_assignable<
                Signal<std::vector<KeyboardInput>>,
                decltype(k)
            >::value,
            "");

    auto t = reactive::get<widget::Theme>(w);

    static_assert(std::is_same<
            signal2::Signal<widget::Theme const&,
                signal::Constant<widget::Theme>
            >,
            decltype(t)
            >::value,
            "");
}

TEST(Widget, set)
{
    auto w =
        reactive::makeWidget(signal::constant(ase::Vector2f(100.0f, 100.0f)));

    auto w2 = reactive::set(std::move(w), signal::constant(avg::Drawing()));
    static_assert(IsWidget<decltype(w2)>::value, "");

    auto w3 = reactive::set(std::move(w2),
            signal::constant(std::vector<InputArea>()));
    static_assert(IsWidget<decltype(w3)>::value, "");

    auto w4 = reactive::set(std::move(w3),
            signal::share(signal::constant(avg::Obb(ase::Vector2f(20.0f, 10.0f)))));
    static_assert(IsWidget<decltype(w4)>::value, "");
    EXPECT_EQ(ase::Vector2f(20.0f, 10.0f), w4.getSize().evaluate());

    auto w5 = reactive::set(std::move(w4),
            signal::constant(std::vector<KeyboardInput>({
                    avg::Obb(ase::Vector2f(30.0f, 2.0f))})));
    static_assert(IsWidget<decltype(w5)>::value, "");
    EXPECT_EQ(1u, w5.getKeyboardInputs().evaluate().size());
    EXPECT_EQ(ase::Vector2f(30.0f, 2.0f),
            w5.getKeyboardInputs().evaluate().at(0).getObb().getSize());

    auto w6 = reactive::set(std::move(w5),
            signal::constant(widget::Theme()));
    static_assert(IsWidget<decltype(w6)>::value, "");

    static_assert(IsTuple<std::tuple<int, std::string>>::value, "");
    static_assert(!IsTuple<std::string>::value, "");

    auto w7 = reactive::set(std::move(w6),
            signal::constant(
                std::make_tuple(
                    widget::Theme(),
                    avg::Obb()
                    )
                )
            );
    static_assert(IsWidget<decltype(w7)>::value, "");
}

TEST(Widget, makeWidgetMap)
{
    auto w =
        reactive::makeWidget(signal::constant(ase::Vector2f(100.0f, 100.0f)));

    auto m = reactive::makeWidgetMap<avg::Obb>([](avg::Obb)
    {
        return avg::Drawing();
    });

    static_assert(IsWidgetMap<decltype(m)>::value, "");

    auto w2 = m(std::move(w));

    static_assert(IsWidget<decltype(w2)>::value, "");
}

TEST(Widget, makeWidgetMapTuple)
{
    auto w =
        reactive::makeWidget(signal::constant(ase::Vector2f(100.0f, 100.0f)));

    auto m = reactive::makeWidgetMap<avg::Obb>([](avg::Obb)
    {
        return std::make_tuple(
            avg::Drawing(),
            avg::Obb(ase::Vector2f(10.0f, 5.0f))
            );
    });

    static_assert(IsWidgetMap<decltype(m)>::value, "");

    auto w2 = m(std::move(w));

    static_assert(IsWidget<decltype(w2)>::value, "");

    EXPECT_EQ(ase::Vector2f(10.0f, 5.0f), std::move(w2).getSize().evaluate());
}

TEST(Widget, cache)
{
    auto w =
        reactive::makeWidget(signal::constant(ase::Vector2f(100.0f, 100.0f)));

    auto w2 = detail::doShare(
            std::move(w),
            btl::TypeList<avg::Drawing, avg::Obb>()
            );

    static_assert(IsSharedSignal<decltype(w2.getDrawing())>::value, "");
    static_assert(IsSharedSignal<decltype(w2.getObb())>::value, "");
    static_assert(!IsSharedSignal<decltype(w2.getAreas())>::value, "");
    static_assert(!IsSharedSignal<decltype(w2.getKeyboardInputs())>::value, "");
    static_assert(!IsSharedSignal<decltype(w2.getTheme())>::value, "");
}

TEST(Widget, operatorPipe)
{
    auto w =
        reactive::makeWidget(signal::constant(ase::Vector2f(100.0f, 100.0f)));

    auto w2 = std::move(w)
        | makeWidgetMap<ase::Vector2f>([](auto)
        {
            return std::make_tuple(
                avg::Obb(ase::Vector2f(10.0f, 20.0f)),
                avg::Drawing()
                );
        })
        | makeWidgetMap<widget::Theme>([](auto)
        {
            return avg::Drawing();
        })
    ;

    EXPECT_EQ(ase::Vector2f(10.0f, 20.0f), std::move(w2).getSize().evaluate());
}

