#pragma once

#include "widget/theme.h"

#include "keyboardinput.h"
#include "inputarea.h"

#include "signal/map.h"
#include "signal/share.h"
#include "signal.h"

#include <avg/drawing.h>
#include <avg/obb.h>
#include <avg/transform.h>

#include <btl/tupleforeach.h>
#include <btl/option.h>

namespace reactive
{
    class InputArea;

    namespace detail
    {
        template <typename T>
        std::vector<T> join(std::vector<std::vector<T>> vv)
        {
            std::vector<T> r;
            for (auto&& v : vv)
            {
                for (auto&& t : v)
                    r.push_back(std::move(t));
            }

            return r;
        }

        template <typename T, typename... Ts>
        std::vector<T> join(std::tuple<std::vector<T>, std::vector<Ts>...> vv)
        {
            std::vector<T> r;
            btl::tuple_foreach(std::move(vv), [&r](auto&& v)
            //for (auto&& v : vv))
            {
                for (auto&& t : v)
                    r.push_back(std::move(t));
            });

            return r;
        }

        template <typename T>
        std::vector<T> concat(std::vector<T> v1, std::vector<T> v2)
        {
            std::vector<T> r = std::move(v1);
            r.reserve(r.size() + v2.size());

            for (auto&& t : v2)
                r.push_back(std::move(t));

            return r;
        }

        inline auto makeKeyboardInputs(Signal<avg::Obb> obb)
        {
            auto focus = signal::input(false);
            auto focusHandle = focus.handle;

            return signal::map([focusHandle=std::move(focusHandle)]
                    (bool focus, avg::Obb obb)
                    -> std::vector<KeyboardInput>
                {
                    return {
                        KeyboardInput(std::move(obb))
                            .setFocus(focus)
                            .setFocusHandle(focusHandle)
                    };
                }, std::move(focus.signal), std::move(obb));
        }
    } // detail

    template <typename TDrawing, typename TAreas, typename TObb,
             typename TKeyboardInputs, typename TTheme>
    class Wid;

    template <typename TDrawing, typename TAreas, typename TObb,
             typename TKeyboardInputs, typename TTheme,
             typename = std::enable_if_t<
                btl::All<
                    IsSignalType<TDrawing, avg::Drawing>,
                    IsSignalType<TAreas, std::vector<InputArea>>,
                    IsSignalType<TObb, avg::Obb>,
                    IsSignalType<TKeyboardInputs, std::vector<KeyboardInput>>,
                    IsSignalType<TTheme, widget::Theme>
                >::value
            >>
    auto makeWidget(TDrawing drawing, TAreas areas, TObb obb,
            TKeyboardInputs keyboardInputs, TTheme theme)
    {
        return Wid<
            std::decay_t<TDrawing>,
            std::decay_t<TAreas>,
            std::decay_t<TObb>,
            std::decay_t<TKeyboardInputs>,
            std::decay_t<TTheme>
                >(
                std::move(drawing),
                std::move(areas),
                std::move(obb),
                std::move(keyboardInputs),
                std::move(theme)
                );
    }

    using WidgetBase = Wid<
        Signal<avg::Drawing>,
        Signal<std::vector<InputArea>>,
        Signal<avg::Obb>,
        Signal<std::vector<KeyboardInput>>,
        Signal<widget::Theme>
        >;

    template <typename TSignalSize, typename = typename
        std::enable_if<
            IsSignalType<TSignalSize, avg::Vector2f>::value
        >::type>
    auto makeWidget(TSignalSize size) -> decltype(auto)
    {
        auto obb = share(signal::map([](avg::Vector2f size)
                {
                    return avg::Obb(size);
                }, std::move(size)));

        auto keyboardInputs = detail::makeKeyboardInputs(obb);

        return makeWidget(
                signal::constant(avg::Drawing()),
                signal::constant(std::vector<InputArea>()),
                std::move(obb),
                std::move(keyboardInputs),
                signal::constant(widget::Theme())
                );

    }

    template <typename TDrawing, typename TAreas, typename TObb,
             typename TKeyboardInputs, typename TTheme>
    auto copy(Wid<TDrawing, TAreas, TObb, TKeyboardInputs, TTheme> const& w)
    {
        return Wid<TDrawing, TAreas, TObb, TKeyboardInputs, TTheme>(w);
    }

    template <typename TDrawing, typename TAreas, typename TObb,
             typename TKeyboardInputs, typename TTheme>
    class Wid
    {
    public:
        Wid(TDrawing drawing, TAreas areas, TObb obb,
                TKeyboardInputs keyboardInputs, TTheme theme) :
            drawing_(std::move(drawing)),
            areas_(std::move(areas)),
            obb_(std::move(obb)),
            keyboardInputs_(std::move(keyboardInputs)),
            theme_(std::move(theme))
        {
        }

        btl::decay_t<TDrawing> getDrawing() const
        {
            return btl::clone(*drawing_);
        }

        btl::decay_t<TAreas> getAreas() const
        {
            return btl::clone(*areas_);
        }

        btl::decay_t<TObb> getObb() const
        {
            return btl::clone(*obb_);
        }

        auto getSize() const
        {
            return signal::map([](avg::Obb const& obb)
                    {
                        return obb.getSize();
                    }, btl::clone(*obb_));
        }

        btl::decay_t<TKeyboardInputs> getKeyboardInputs() const
        {
            return btl::clone(*keyboardInputs_);
        }

        auto hasFocus() const
        {
            return signal::map([](std::vector<KeyboardInput> const& inputs)
                {
                    for (auto&& input : inputs)
                        if (input.hasFocus())
                            return true;

                    return false;
                }, getKeyboardInputs());
        }

        btl::decay_t<TTheme> getTheme() const
        {
            return btl::clone(*theme_);
        }

        template <typename T, typename = std::enable_if_t<
            IsSignalType<T, widget::Theme>::value
            >
        >
        auto setTheme(T theme) &&
        {
            return makeWidget(
                    std::move(*drawing_),
                    std::move(*areas_),
                    std::move(*obb_),
                    std::move(*keyboardInputs_),
                    std::move(theme)
                    );
        }

        template <typename TSignalDrawing, typename = std::enable_if_t<
            IsSignalType<TSignalDrawing, avg::Drawing>::value
            >
        >
        auto setDrawing(TSignalDrawing drawing) &&
        {
            return makeWidget(
                    std::move(drawing),
                    std::move(*areas_),
                    std::move(*obb_),
                    std::move(*keyboardInputs_),
                    std::move(*theme_)
                    );
        }

        template <typename TSignalAreas, typename = std::enable_if_t<
            IsSignalType<TSignalAreas, std::vector<InputArea>>::value
            >
        >
        auto setAreas(TSignalAreas areas) &&
        {
            return makeWidget(
                    std::move(*drawing_),
                    std::move(areas),
                    std::move(*obb_),
                    std::move(*keyboardInputs_),
                    std::move(*theme_)
                    );
        }

        template <typename T>
        auto transform(Signal<avg::Transform, T> t) &&
        {
            auto tr = signal::share(std::move(t));

            auto drawing = signal::map([](avg::Drawing d, avg::Transform t)
                    -> avg::Drawing
                {
                    return t * std::move(d);
                },
                std::move(*drawing_), tr);

            auto areas = signal::map([](std::vector<InputArea> areas,
                        avg::Transform const& t)
                    -> std::vector<InputArea>
                {
                    for (auto&& a : areas)
                        a = std::move(a).transform(t);
                    return areas;
                }, std::move(*areas_), tr);

            auto obb = signal::map([](avg::Obb const& obb, avg::Transform const& t)
                    -> avg::Obb
                {
                    return t * obb;
                }, std::move(*obb_), tr);

            auto keyboardInputs = signal::map([](std::vector<KeyboardInput> inputs,
                        avg::Transform const& t) -> std::vector<KeyboardInput>
                {
                    for (auto&& input : inputs)
                    {
                        input = std::move(input).transform(t);
                    }

                    return inputs;
                }, std::move(*keyboardInputs_), tr);

            return std::move(*this)
                .setDrawing(std::move(drawing))
                .setAreas(std::move(areas))
                .setObb(std::move(obb))
                .setKeyboardInputs(std::move(keyboardInputs))
                ;
        }

        template <typename TSignalTransform, typename = std::enable_if_t<
            IsSignalType<TSignalTransform, avg::Transform>::value
            >
        >
        auto transformR(TSignalTransform t) &&
        {
            static_assert(IsSignal<decltype(t)>::value, "");
            auto t2 = signal::map([](avg::Obb const& obb, avg::Transform const& t)
                    -> avg::Transform
            {
                return obb.getTransform() * t * obb.getTransform().inverse();
            }, obb_->clone(), std::move(t));

            return std::move(*this).transform(std::move(t2));
        }

        template <typename TSignalObb, typename =
            std::enable_if_t<
                IsSignalType<TSignalObb, avg::Obb>::value
            >>
        auto setObb(TSignalObb obb) &&
        {
            return makeWidget(
                    std::move(*drawing_),
                    std::move(*areas_),
                    std::move(obb),
                    std::move(*keyboardInputs_),
                    std::move(*theme_)
                    );
        }

        template <typename TSignalKeyboardInputs, typename =
            std::enable_if_t<
                IsSignalType<
                    TSignalKeyboardInputs,
                    std::vector<KeyboardInput>
                >::value
            >>
        auto setKeyboardInputs(TSignalKeyboardInputs inputs) &&
        {
            return makeWidget(
                    std::move(*drawing_),
                    std::move(*areas_),
                    std::move(*obb_),
                    std::move(inputs),
                    std::move(*theme_)
                    );
        }

        btl::option<signal_time_t> update(signal::FrameInfo const& frame)
        {
            auto t = drawing_->updateBegin(frame);
            auto t2 = areas_->updateBegin(frame);
            t = min(t, t2);

            t2 = obb_->updateBegin(frame);
            t = min(t, t2);

            t2 = keyboardInputs_->updateBegin(frame);
            t = min(t, t2);

            t2 = theme_->updateBegin(frame);
            t = min(t, t2);

            t2 = drawing_->updateEnd(frame);
            t = min(t, t2);

            t2 = areas_->updateEnd(frame);
            t = min(t, t2);

            t2 = obb_->updateEnd(frame);
            t = min(t, t2);

            t2 = keyboardInputs_->updateEnd(frame);
            t = min(t, t2);

            t2 = theme_->updateEnd(frame);
            t = min(t, t2);

            return t;
        }

        operator WidgetBase() &&
        {
            return WidgetBase(
                    std::move(*drawing_),
                    std::move(*areas_),
                    std::move(*obb_),
                    std::move(*keyboardInputs_),
                    std::move(*theme_)
                    );
        }

        template <typename TWidgetMap>
        auto operator|(TWidgetMap&& widgetMap) &&
        -> decltype(
                std::forward<TWidgetMap>(widgetMap)(std::move(*this))
                )
        {
            return std::forward<TWidgetMap>(widgetMap)(std::move(*this));
        }

        Wid clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<btl::decay_t<TDrawing>> drawing_;
        btl::CloneOnCopy<btl::decay_t<TAreas>> areas_;
        btl::CloneOnCopy<btl::decay_t<TObb>> obb_;
        btl::CloneOnCopy<btl::decay_t<TKeyboardInputs>> keyboardInputs_;
        btl::CloneOnCopy<btl::decay_t<TTheme>> theme_;
    };

    struct Widget : WidgetBase
    {
        inline Widget(WidgetBase base) :
            WidgetBase(std::move(base))
        {
        }

        template <typename TDrawing, typename TAreas, typename TObb,
                typename TKeyboardInputs, typename TTheme, typename... Ts>
        Widget(Wid<TDrawing, TAreas, TObb, TKeyboardInputs, TTheme> base) :
            WidgetBase(std::move(base))
        {
        }

        inline Widget clone() const
        {
            return WidgetBase::clone();
        }
    };

    template <typename T>
    using IsWidget = std::is_convertible<T, WidgetBase>;
} // reactive

