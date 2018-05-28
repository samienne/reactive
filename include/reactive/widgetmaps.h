#pragma once

#include "clickevent.h"
#include "widgetmap.h"
#include "rendering.h"

#include <reactive/stream/collect.h>
#include <reactive/stream/stream.h>

#include <reactive/signal/map.h>
#include <reactive/signal.h>

#include <btl/sequence.h>
#include <btl/bundle.h>

namespace reactive
{
    template <typename TSignal>
    auto addDrawing(TSignal drawing)
    {
        return makeWidgetMap<avg::Obb, avg::Drawing>(
                [d1=std::move(drawing)]
                (auto obb, auto d2)
                {
                    return std::move(d1) + (obb.getTransform() * std::move(d2));
                });
    }

    template <typename TSignalDrawings, typename = std::enable_if_t<
        btl::IsSequence<SignalType<TSignalDrawings>>::value
        >
    >
    auto addDrawings(TSignalDrawings drawings)
    {
        return makeWidgetMap<ObbTag, DrawingTag>(
                []
                (avg::Obb const& obb, auto d1, auto drawings)
                //-> std::tuple<avg::Obb, avg::Drawing>
                -> avg::Drawing
                {
                    avg::Drawing r = std::move(d1);
                    //for (auto&& d : drawings)
                    btl::forEach(std::move(drawings), [&r, &obb](auto d)
                    {
                        r = std::move(r) + (obb.getTransform() * std::move(d));
                    });

                    //return std::make_tuple(std::move(obb), std::move(r));
                    return r;
                },
                std::move(drawings)
                );
    }

    template <typename... Ts, typename TFunc, typename... Us,
             typename = std::enable_if_t
        <
            std::is_assignable<
                std::function<avg::Drawing(typename Ts::Type..., SignalType<Us>...)>,
                TFunc
            >::value
        >
    >
    auto onDraw(TFunc&& func, Us... us)
    {
        return makeWidgetMap<DrawingTag, ObbTag, Ts...>(
                [func=std::forward<TFunc>(func)]
                (auto d1, avg::Obb const& obb, auto&&... ts) -> avg::Drawing
                {
                    auto d2 = func(
                        std::forward<decltype(ts)>(ts)...
                        );

                    return std::move(d1) + (obb.getTransform() * std::move(d2));
                },
                std::move(us)...
                );
    }

    template <typename... Ts, typename TFunc, typename... Us,
             typename = std::enable_if_t
        <
            std::is_assignable<
                std::function<avg::Drawing(typename Ts::Type..., SignalType<Us>...)>,
                TFunc
            >::value
        >
    >
    auto onDrawBehind(TFunc&& func, Us... us)
    {
        return makeWidgetMap<DrawingTag, ObbTag, Ts...>(
                [func=std::forward<TFunc>(func)]
                (auto d1, avg::Obb const& obb, auto&&... ts) -> avg::Drawing
                {
                    auto d2 = func(
                        std::forward<decltype(ts)>(ts)...
                        );

                    return (obb.getTransform() * std::move(d2)) + std::move(d1);
                },
                std::move(us)...
                );
    }

    inline auto drawBackground(avg::Vector2f size, avg::Brush const brush)
        -> avg::Drawing
    {
        auto t = avg::Transform()
            .translate(0.5f * size[0], 0.5f * size[1]);

        return t * avg::Drawing(makeShape(
                    makeRect(size[0], size[1]),
                    btl::just(brush),
                    btl::none));
    }

    inline auto background(Signal<avg::Brush> brush)
        // -> FactoryMap;
    {
        return onDrawBehind<SizeTag>(
                    drawBackground, std::move(brush));
    }

    inline auto background()
        // -> FactoryMap;
    {
        return onDrawBehind<SizeTag, ThemeTag>(
                [](auto size, auto const& theme)
                {
                    return drawBackground(size, theme.getBackground());
                });
    }

    inline auto onPointerDown(Signal<
            std::function<void(ase::PointerButtonEvent const&)>
            > cb)
    {
        btl::UniqueId id = btl::makeUniqueId();

        return makeWidgetMap<InputAreaTag, ObbTag>(
            [id](std::vector<InputArea> areas, avg::Obb const& obb, auto cb)
            -> std::vector<InputArea>
            {
                if (!areas.empty()
                        && areas.back().getObbs().size() == 1
                        && areas.back().getObbs().front() == obb)
                {
                    areas.back() = std::move(areas.back()).onDown(std::move(cb));
                    return areas;
                }

                areas.push_back(
                        makeInputArea(id, obb).onDown(std::move(cb))
                        );

                return areas;
            },
            std::move(cb)
            );
    }

    inline auto onPointerDown(
            std::function<void(ase::PointerButtonEvent const&)> cb
            )
    {
        return onPointerDown(signal::constant(std::move(cb)));
    }

    template <typename T, typename U, typename = std::enable_if_t<
        std::is_convertible<
            T,
            std::function<void(ase::PointerButtonEvent const&)>
        >::value
        >>
    inline auto onPointerUp(Signal<T, U> cb)
            //std::function<void(ase::PointerButtonEvent const&)>> cb)
    {
        auto id = btl::makeUniqueId();

        return makeWidgetMap<InputAreaTag, ObbTag>(
            [id](std::vector<InputArea> areas, avg::Obb const& obb, auto cb)
            -> std::vector<InputArea>
            {
                if (!areas.empty()
                        && areas.back().getObbs().size() == 1
                        && areas.back().getObbs().front() == obb)
                {
                    areas.back() = std::move(areas.back()).onUp(std::move(cb));
                    return areas;
                }

                areas.push_back(
                        makeInputArea(id, obb).onUp(std::move(cb))
                        );

                return areas;
            },
            std::move(cb)
            );
    }

    inline auto onPointerUp(
            std::function<void(ase::PointerButtonEvent const&)> cb)
    {
        return onPointerUp(signal::constant(std::move(cb)));
    }

    inline auto onPointerUp(std::function<void()> cb)
    {
        std::function<void(ase::PointerButtonEvent const& e)> f =
            std::bind(std::move(cb));
        return onPointerUp(f);
    }

    inline auto onPointerMove(Signal<
            std::function<void(ase::PointerMoveEvent const&)>
            > cb)
    {
        auto id = btl::makeUniqueId();

        return makeWidgetMap<InputAreaTag, ObbTag>(
            [id](std::vector<InputArea> areas, avg::Obb const& obb, auto cb)
            -> std::vector<InputArea>
            {
                if (!areas.empty()
                        && areas.back().getObbs().size() == 1
                        && areas.back().getObbs().front() == obb)
                {
                    areas.back() = std::move(areas.back()).onMove(std::move(cb));
                    return areas;
                }

                areas.push_back(
                        makeInputArea(id, obb).onMove(std::move(cb))
                        );

                return areas;
            },
            std::move(cb)
            );
    }

    inline auto onPointerMove(
            std::function<void(ase::PointerMoveEvent const&)> cb
            )
    {
        return onPointerMove(signal::constant(std::move(cb)));
    }

    inline auto onHover(Signal<
            std::function<void(reactive::HoverEvent const&)>
            > cb)
    {
        auto id = btl::makeUniqueId();

        return makeWidgetMap<InputAreaTag, ObbTag>(
            [id](std::vector<InputArea> areas, avg::Obb const& obb, auto cb)
            -> std::vector<InputArea>
            {
                if (!areas.empty()
                        && areas.back().getObbs().size() == 1
                        && areas.back().getObbs().front() == obb)
                {
                    areas.back() = std::move(areas.back()).onHover(std::move(cb));
                    return areas;
                }

                areas.push_back(
                        makeInputArea(id, obb).onHover(std::move(cb))
                        );

                return areas;
            },
            std::move(cb)
            );
    }

    inline auto onHover(
            std::function<void(reactive::HoverEvent const&)> cb
            )
    {
        return onHover(signal::constant(std::move(cb)));
    }


    template <typename TSignalHandler>
    auto onKeyEvent(TSignalHandler handler)
    {
        return makeWidgetMap<KeyboardInputTag>([]
                (auto inputs, auto const& handler)
                -> std::vector<KeyboardInput>
                {
                    for (auto&& input : inputs)
                        input = std::move(input)
                            .onKeyEvent(std::move(handler));
                    return inputs;
                },
                std::move(handler)
                );
    }

    inline auto onKeyEvent(KeyboardInput::Handler handler)
    {
        return onKeyEvent(signal::constant(std::move(handler)));
    }

    template <typename TSignalTransform, typename =
        std::enable_if_t<
            IsSignalType<TSignalTransform, avg::Transform>::value
        >>
    inline auto transform(TSignalTransform t)
    {
        auto tt = btl::cloneOnCopy(std::move(t));
        static_assert(std::is_copy_constructible<decltype(tt)>::value, "");
        auto f = [t=std::move(tt)](auto widget)
        {
            auto w = std::move(widget)
                //.transform(*std::move(t))
                .transform(t->clone())
                ;

            return w;
        };

        static_assert(IsWidgetMap<decltype(f)>::value, "");

        return mapWidget(std::move(f));
    }

    template <typename T, typename U, std::enable_if_t<
        std::is_convertible<T, std::function<void(ClickEvent const&)>>::value
        , int> = 0>
    inline auto onClick(unsigned int button, Signal<T, U> cb)
            //Signal<std::function<void(ClickEvent const&)>> cb)
    {
        auto f = [button](
                std::function<void(ClickEvent const&)> const& cb,
                ase::PointerButtonEvent const& e)
        {
            if (button == 0 || e.button == button)
                cb(ClickEvent(e.pointer, e.button, e.pos));
        };

        return onPointerUp(signal::mapFunction(std::move(f), std::move(cb)));
    }

    template <typename T, typename U, std::enable_if_t<
        std::is_convertible<T, std::function<void()>>::value
        , int> = 0>
    inline auto onClick(unsigned int button, Signal<T, U> cb)
    {
        auto f = [](std::function<void()> cb, ClickEvent const&)
        {
            cb();
        };

        return onClick(button, signal::mapFunction(std::move(f), std::move(cb)));
    }

    inline auto onClick(unsigned int button, std::function<void(ClickEvent const&)> f)
    {
        auto g = [button, f = std::move(f)](ase::PointerButtonEvent const& e)
        {
            if (button == 0 || e.button == button)
                f(ClickEvent(e.pointer, e.button, e.pos));
        };

        return onPointerUp(g);
    }

    template <typename TWidgets, std::enable_if_t<
        btl::All<
            btl::IsSequence<TWidgets>
            >::value
        , int> = 0
    >
    auto addWidgets(TWidgets widgets)
    {
        auto f = [widgets=btl::cloneOnCopy(std::move(widgets))](auto widget)
        {
            auto addAreas = [](std::vector<InputArea> own,
                    //std::vector<std::vector<InputArea>> areas
                    auto areas
                    )
                -> std::vector<InputArea>
                {
                    return detail::concat(own, detail::join(std::move(areas)));
                };

            //std::vector<Signal<std::vector<InputArea>>> areas;
            //for (auto&& w : *widgets)
            auto areas = btl::fmap(*widgets, [](auto&& w)
            {
                return w.getAreas();
            });

            auto areasSignal = signal::map(
                    addAreas,
                    widget.getAreas(),
                    signal::combine(std::move(areas))
                    );

            //std::vector<Signal<std::vector<KeyboardInput>>> vectorOfInputs;
            //vectorOfInputs.reserve(widgets->size());
            //for (auto&& w : *widgets)
            auto tupleOfInputs = btl::fmap(*widgets, [](auto&& w)
            {
                return w.getKeyboardInputs();
            });

            auto flatten = [](auto inputs)
            {
                return detail::join(std::move(inputs));
            };

            auto inputs = signal::map(flatten,
                    signal::combine(std::move(tupleOfInputs)));

            auto addInputs = [](std::vector<KeyboardInput> lhs,
                    std::vector<KeyboardInput> rhs)
                -> std::vector<KeyboardInput>
                {
                    std::vector<KeyboardInput> result;

                    for (auto&& input : lhs)
                    {
                        if (input.isFocusable())
                            result.push_back(std::move(input));
                    }

                    for (auto&& input : rhs)
                    {
                        if (input.isFocusable())
                            result.push_back(std::move(input));
                    }

                    return result;
                };

            auto keyboardInputsSignal = signal::map(addInputs,
                    std::move(widget.getKeyboardInputs()),
                    std::move(inputs));

            //std::vector<Signal<avg::Drawing>> drawings;
            //for (auto&& w : *widgets)
                //drawings.push_back(std::move(w.getDrawing()));
            auto drawings = btl::fmap(*widgets, [](auto&& w)
            {
                return w.getDrawing();
            });

            return makeWidget(
                    std::move(widget.getDrawing()),
                    std::move(areasSignal),
                    std::move(widget.getObb()),
                    std::move(keyboardInputsSignal),
                    std::move(widget.getTheme())
                    )
                | addDrawings(signal::combine(std::move(drawings)))
                ;

        };

        return mapWidget(std::move(f));
    }

    template <typename TSignalWidget, typename = typename
        std::enable_if
        <
            IsSignalType<TSignalWidget, Widget>::value
        >::type>
    auto reduce(TSignalWidget w2)
    {
        auto w = signal::share(std::move(w2));

        auto drawing = signal::mbind([](auto&& w) {
                return w.getDrawing().clone(); }, w);
        auto areas = signal::mbind([](auto&& w) { return w.getAreas().clone(); }, w);
        auto obb = signal::mbind([](auto&& w) { return w.getObb().clone(); }, w);
        auto keyboardInputs = signal::mbind([](auto&& w) {
                return w.getKeyboardInputs().clone(); }, w);
        auto theme = signal::mbind([](auto&& w) { return w.getTheme().clone(); }, w);

        return makeWidget(
                std::move(drawing),
                std::move(areas),
                std::move(obb),
                std::move(keyboardInputs),
                std::move(theme));
    }


    template <typename TSignalWidgets, std::enable_if_t<
            IsSignalType<TSignalWidgets, std::vector<Widget>>::value
        , int> = 0
    >
    auto addWidgets(TSignalWidgets widgets)
    {
        auto f = [widgets=cloneOnCopy(std::move(widgets))](auto widget)
        {
            auto w1 = signal::map([widget=std::move(widget)]
                    (std::vector<Widget> widgets)
                    {
                        return btl::clone(widget)
                            | addWidgets(std::move(widgets))
                            ;
                        //return btl::clone(widget);
                    },
                    btl::clone(*widgets)
                    );

            return reduce(std::move(w1));
        };

        return mapWidget(std::move(f));
    }

    template <typename TSignalBool, typename = std::enable_if_t<
        IsSignalType<TSignalBool, bool>::value
        >
    >
    auto setFocusable(TSignalBool focusable)
    {
        return makeWidgetMap<KeyboardInputTag>(
                []
                (std::vector<KeyboardInput> inputs, bool focusable)
                -> std::vector<KeyboardInput>
                {
                    inputs[0] = std::move(inputs[0]).setFocusable(focusable);
                    return inputs;
                },
                std::move(focusable)
                );
    }

    template <typename TSignalBool, typename = std::enable_if_t<
        IsSignalType<TSignalBool, bool>::value
        >
    >
    auto requestFocus(TSignalBool requestFocus)
    {
        return makeWidgetMap<KeyboardInputTag>(
                []
                (std::vector<KeyboardInput> inputs, bool requestFocus)
                -> std::vector<KeyboardInput>
                {
                    if (!inputs[0].hasFocus() || requestFocus)
                        inputs[0] = std::move(inputs[0]).requestFocus(requestFocus);

                    return inputs;
                },
                std::move(requestFocus)
                );
    }

    inline auto focusOn(stream::Stream<bool> stream)
    {
        auto hasValues = [](std::vector<bool> const& v) -> bool
        {
            return !v.empty();
        };

        auto focusRequest = signal::map(
                hasValues,
                stream::collect(std::move(stream))
                );

        auto f = [focusRequest=btl::cloneOnCopy(std::move(focusRequest))]
            (auto widget)
        {
            return std::move(widget)
                | setFocusable(signal::constant(true))
                //| requestFocus(std::move(*focusRequest))
                | requestFocus(focusRequest->clone())
                ;
        };

        return mapWidget(std::move(f));
    }
} // reactive

