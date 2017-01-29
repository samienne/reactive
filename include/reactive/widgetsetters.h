#pragma once

#include "widgettraits.h"
#include "widget.h"

#include "reactive/signal/share.h"
#include "reactive/signal/split.h"

#include <avg/drawing.h>

#include <btl/tuplereduce.h>
#include <btl/typetraits.h>

namespace reactive
{
    template <typename T, typename = void>
    struct WidgetSetter
    {
        template <typename TWidget>
        void set()
        {
        }
    };

    template <typename TDrawing>
    struct WidgetSetter<TDrawing, std::enable_if_t<
        IsSignalType<TDrawing, avg::Drawing>::value
        >
    >
    {
        template <typename TWidget>
        auto set(TWidget&& widget, TDrawing&& drawing)
        {
            auto ret = std::forward<TWidget>(widget).setDrawing(
                    std::forward<TDrawing>(drawing));

            static_assert(IsWidget<decltype(ret)>::value, "");
            return ret;
        }
    };

    template <typename TAreas>
    struct WidgetSetter<TAreas, std::enable_if_t<
        IsSignalType<TAreas, std::vector<InputArea>>::value
        >
    >
    {
        template <typename TWidget>
        auto set(TWidget&& widget, TAreas&& areas)
        {
            auto ret = std::forward<TWidget>(widget).setAreas(
                    std::forward<TAreas>(areas));

            static_assert(IsWidget<decltype(ret)>::value, "");
            return ret;
        }
    };

    template <typename TObb>
    struct WidgetSetter<TObb, std::enable_if_t<
        IsSignalType<TObb, avg::Obb>::value
        >
    >
    {
        template <typename TWidget>
        auto set(TWidget&& widget, TObb&& obb)
        {
            auto ret = std::forward<TWidget>(widget).setObb(
                    std::forward<TObb>(obb));

            static_assert(IsWidget<decltype(ret)>::value, "");
            return ret;
        }
    };

    template <typename TKeyboardInputs>
    struct WidgetSetter<TKeyboardInputs, std::enable_if_t<
        IsSignalType<TKeyboardInputs, std::vector<KeyboardInput>>::value
        >
    >
    {
        template <typename TWidget>
        auto set(TWidget&& widget, TKeyboardInputs&& keyboardInput)
        {
            auto ret = std::forward<TWidget>(widget).setKeyboardInputs(
                    std::forward<TKeyboardInputs>(keyboardInput));

            static_assert(IsWidget<decltype(ret)>::value, "");
            return ret;
        }
    };

    template <typename TTheme>
    struct WidgetSetter<TTheme, std::enable_if_t<
        IsSignalType<TTheme, widget::Theme>::value
        >
    >
    {
        template <typename TWidget>
        auto set(TWidget&& widget, TTheme&& theme)
        {
            auto ret = std::forward<TWidget>(widget).setTheme(
                    std::forward<TTheme>(theme));

            static_assert(IsWidget<decltype(ret)>::value, "");
            return ret;
        }
    };

    template <typename T>
    struct IsTuple : std::false_type {};

    template <typename... Ts>
    struct IsTuple<std::tuple<Ts...>> : std::true_type {};

    template <typename TSignalTuple>
    struct WidgetSetter<TSignalTuple, std::enable_if_t<
        IsTuple<std::decay_t<SignalType<TSignalTuple>>>::value
        >
    >
    {
        template <typename TWidget>
        auto set(TWidget&& widget, TSignalTuple&& values)
        {
            auto ret = btl::tuple_reduce(
                    std::forward<TWidget>(widget),
                    signal::split(std::forward<TSignalTuple>(values)),
                    [](auto widget, auto value)
                    {
                        return WidgetSetter<decltype(value)>().set(
                            std::move(widget), std::move(value));
                    });

            static_assert(IsWidget<decltype(ret)>::value, "");
            return ret;
        }
    };

    template <typename TWidget, typename TValue>
    auto set(TWidget&& widget, TValue&& value)
        -> decltype(
                WidgetSetter<std::decay_t<TValue>>().set(
                    std::forward<TWidget>(widget),
                    std::forward<TValue>(value)
                    )
                )
    {
        return WidgetSetter<std::decay_t<TValue>>().set(
                std::forward<TWidget>(widget),
                std::forward<TValue>(value)
                );
    }
} // reactive

