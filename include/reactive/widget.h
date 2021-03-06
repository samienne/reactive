#pragma once

#include "widget/theme.h"

#include "keyboardinput.h"
#include "inputarea.h"
#include "drawcontext.h"

#include "signal/map.h"
#include "signal/share.h"
#include "signal/signal.h"

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

        inline auto makeKeyboardInputs(AnySignal<avg::Obb> obb)
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

    template <typename TDrawContext, typename TDrawing, typename TAreas,
             typename TObb, typename TKeyboardInputs, typename TTheme>
    class Wid;

    template <typename TDrawContext, typename TDrawing, typename TAreas,
             typename TObb, typename TKeyboardInputs, typename TTheme,
             typename = std::enable_if_t<
                btl::All<
                    signal::IsSignalType<TDrawContext, DrawContext>,
                    signal::IsSignalType<TDrawing, avg::Drawing>,
                    signal::IsSignalType<TAreas, std::vector<InputArea>>,
                    signal::IsSignalType<TObb, avg::Obb>,
                    signal::IsSignalType<TKeyboardInputs, std::vector<KeyboardInput>>,
                    signal::IsSignalType<TTheme, widget::Theme>
                >::value
            >>
    auto makeWidget(TDrawContext drawContext, TDrawing drawing, TAreas areas,
            TObb obb, TKeyboardInputs keyboardInputs, TTheme theme)
    {
        return Wid<
            std::decay_t<TDrawContext>,
            std::decay_t<TDrawing>,
            std::decay_t<TAreas>,
            std::decay_t<TObb>,
            std::decay_t<TKeyboardInputs>,
            std::decay_t<TTheme>
                >(
                std::move(drawContext),
                std::move(drawing),
                std::move(areas),
                std::move(obb),
                std::move(keyboardInputs),
                std::move(theme)
                );
    }

    using WidgetBase = Wid<
        AnySignal<DrawContext>,
        AnySignal<avg::Drawing>,
        AnySignal<std::vector<InputArea>>,
        AnySignal<avg::Obb>,
        AnySignal<std::vector<KeyboardInput>>,
        AnySignal<widget::Theme>
        >;

    template <typename T, typename U>
    auto makeWidget(
            Signal<T, DrawContext> drawContext,
            Signal<U, avg::Vector2f> size
            )
    -> decltype(auto)
    {
        auto obb = share(signal::map([](avg::Vector2f size)
                {
                    return avg::Obb(size);
                }, std::move(size)));

        auto keyboardInputs = detail::makeKeyboardInputs(obb);

        auto drawing = signal::map([](DrawContext const& drawContext)
                {
                    return avg::Drawing(drawContext.getResource());
                }, drawContext.clone());

        return makeWidget(
                std::move(drawContext),
                std::move(drawing),
                signal::constant(std::vector<InputArea>()),
                std::move(obb),
                std::move(keyboardInputs),
                signal::constant(widget::Theme())
                );

    }

    template <typename TDrawContext, typename TDrawing, typename TAreas,
             typename TObb, typename TKeyboardInputs, typename TTheme>
    auto copy(Wid<TDrawContext, TDrawing, TAreas, TObb, TKeyboardInputs,
            TTheme> const& w)
    {
        return Wid<TDrawContext, TDrawing, TAreas, TObb, TKeyboardInputs, TTheme>(w);
    }

    template <typename TDrawContext, typename TDrawing, typename TAreas,
             typename TObb, typename TKeyboardInputs, typename TTheme>
    class Wid
    {
    public:
        Wid(TDrawContext drawContext, TDrawing drawing, TAreas areas, TObb obb,
                TKeyboardInputs keyboardInputs, TTheme theme) :
            drawContext_(std::move(drawContext)),
            drawing_(std::move(drawing)),
            areas_(std::move(areas)),
            obb_(std::move(obb)),
            keyboardInputs_(std::move(keyboardInputs)),
            theme_(std::move(theme))
        {
        }

        std::decay_t<TDrawContext> getDrawContext() const
        {
            return btl::clone(*drawContext_);
        }

        btl::decay_t<TDrawing> getDrawing() const
        {
            return btl::clone(*drawing_);
        }

        btl::decay_t<TAreas> getInputAreas() const
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
            signal::IsSignalType<T, widget::Theme>::value
            >
        >
        auto setTheme(T theme) &&
        {
            return makeWidget(
                    std::move(*drawContext_),
                    std::move(*drawing_),
                    std::move(*areas_),
                    std::move(*obb_),
                    std::move(*keyboardInputs_),
                    std::move(theme)
                    );
        }

        template <typename TSignalDrawing, typename = std::enable_if_t<
            signal::IsSignalType<TSignalDrawing, avg::Drawing>::value
            >
        >
        auto setDrawing(TSignalDrawing drawing) &&
        {
            return makeWidget(
                    std::move(*drawContext_),
                    std::move(drawing),
                    std::move(*areas_),
                    std::move(*obb_),
                    std::move(*keyboardInputs_),
                    std::move(*theme_)
                    );
        }

        template <typename TSignalAreas, typename = std::enable_if_t<
            signal::IsSignalType<TSignalAreas, std::vector<InputArea>>::value
            >
        >
        auto setAreas(TSignalAreas areas) &&
        {
            return makeWidget(
                    std::move(*drawContext_),
                    std::move(*drawing_),
                    std::move(areas),
                    std::move(*obb_),
                    std::move(*keyboardInputs_),
                    std::move(*theme_)
                    );
        }

        template <typename T>
        auto transform(Signal<T, avg::Transform> t) &&
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
            signal::IsSignalType<TSignalTransform, avg::Transform>::value
            >
        >
        auto transformR(TSignalTransform t) &&
        {
            auto t2 = signal::map([](avg::Obb const& obb, avg::Transform const& t)
                    -> avg::Transform
            {
                return obb.getTransform() * t * obb.getTransform().inverse();
            }, obb_->clone(), std::move(t));

            return std::move(*this).transform(std::move(t2));
        }

        template <typename TSignalObb, typename =
            std::enable_if_t<
                signal::IsSignalType<TSignalObb, avg::Obb>::value
            >>
        auto setObb(TSignalObb obb) &&
        {
            return makeWidget(
                    std::move(*drawContext_),
                    std::move(*drawing_),
                    std::move(*areas_),
                    std::move(obb),
                    std::move(*keyboardInputs_),
                    std::move(*theme_)
                    );
        }

        template <typename TSignalKeyboardInputs, typename =
            std::enable_if_t<
                signal::IsSignalType<
                    TSignalKeyboardInputs,
                    std::vector<KeyboardInput>
                >::value
            >>
        auto setKeyboardInputs(TSignalKeyboardInputs inputs) &&
        {
            return makeWidget(
                    std::move(*drawContext_),
                    std::move(*drawing_),
                    std::move(*areas_),
                    std::move(*obb_),
                    std::move(inputs),
                    std::move(*theme_)
                    );
        }

        btl::option<signal::signal_time_t> update(signal::FrameInfo const& frame)
        {
            auto t = drawing_->updateBegin(frame);
            auto t2 = areas_->updateBegin(frame);
            t = min(t, t2);

            t2 = drawContext_->updateBegin(frame);
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
                    std::move(*drawContext_),
                    std::move(*drawing_),
                    std::move(*areas_),
                    std::move(*obb_),
                    std::move(*keyboardInputs_),
                    std::move(*theme_)
                    );
        }

        template <typename TWidgetTransform>
        auto operator|(TWidgetTransform&& t) &&
        -> decltype(
            std::forward<TWidgetTransform>(t)(std::move(*this)).first
            )
        {
            return std::forward<TWidgetTransform>(t)(std::move(*this)).first;
        }

        Wid clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<std::decay_t<TDrawContext>> drawContext_;
        btl::CloneOnCopy<std::decay_t<TDrawing>> drawing_;
        btl::CloneOnCopy<std::decay_t<TAreas>> areas_;
        btl::CloneOnCopy<std::decay_t<TObb>> obb_;
        btl::CloneOnCopy<std::decay_t<TKeyboardInputs>> keyboardInputs_;
        btl::CloneOnCopy<std::decay_t<TTheme>> theme_;
    };

    struct Widget : WidgetBase
    {
        inline Widget(WidgetBase base) :
            WidgetBase(std::move(base))
        {
        }

        template <typename TDrawContext, typename TDrawing, typename TAreas,
                 typename TObb, typename TKeyboardInputs, typename TTheme,
                 typename... Ts>
        Widget(Wid<TDrawContext, TDrawing, TAreas, TObb, TKeyboardInputs, TTheme> base) :
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

