#pragma once

#include "bindkeyboardinputs.h"
#include "setkeyboardinputs.h"
#include "widgettransformer.h"

#include "reactive/widgetfactory.h"

#include "reactive/inputresult.h"

#include "reactive/signal/convert.h"
#include "reactive/signal/map.h"
#include <reactive/signal/signal.h>

#include <ase/keyevent.h>

namespace reactive::widget
{
    namespace detail
    {
        template <typename TSignalHandler>
        auto onKeyEvent(TSignalHandler handler)
        {
            return makeWidgetTransformer()
                .compose(grabKeyboardInputs())
                .values(std::move(handler))
                .bind([](auto inputs, auto handler) mutable
                {
                    auto newInputs = signal::map(
                        [](std::vector<KeyboardInput> inputs, auto const& handler)
                        -> std::vector<KeyboardInput>
                        {
                            for (auto&& input : inputs)
                                input = std::move(input)
                                    .onKeyEvent(handler);
                            return inputs;
                        },
                        std::move(inputs),
                        std::move(handler)
                        );

                    return setKeyboardInputs(std::move(newInputs));
                });
        }

        inline auto onKeyEvent(KeyboardInput::Handler handler)
        {
            return onKeyEvent(signal::constant(std::move(handler)));
        }
    }

    class OnKeyEvent
    {
    public:
        inline OnKeyEvent(
                signal::Convert<std::function<bool(ase::KeyEvent const&)>> predicate,
                signal::Convert<std::function<void(ase::KeyEvent const&)>> action) :
            predicate_(std::move(predicate)),
            action_(std::move(action))
        {
        }

        /*
        template <typename TWidgetFactory, typename = typename
            std::enable_if
            <
                !IsWidget<TWidgetFactory>::value &&
                IsWidgetFactory<TWidgetFactory>::value
            >::type>
        inline auto operator()(TWidgetFactory&& factory) const
        {
            auto f = [action=btl::cloneOnCopy(btl::clone(action_)),
                    predicate=btl::cloneOnCopy(btl::clone(predicate_))]
                (auto widget) // -> Widget
            {
                auto g = [](
                    std::function<bool(ase::KeyEvent const&)> const& pred,
                    std::function<void(ase::KeyEvent const&)> const& action,
                    ase::KeyEvent const& event)
                    -> InputResult
                {
                    if (!pred(event))
                        return InputResult::unhandled;

                    action(event);
                    return InputResult::handled;
                };

                return std::move(widget)
                    | detail::onKeyEvent(signal::mapFunction(std::move(g),
                                btl::clone(*predicate), btl::clone(*action)));
            };

            return std::forward<TWidgetFactory>(factory)
                .map(std::move(f));
        }*/

        template <typename TWidget, typename = typename
            std::enable_if
            <
                IsWidget<TWidget>::value
            >::type>
        inline auto operator()(TWidget&& widget, bool = true) const
        {
            auto f = [](
                std::function<bool(ase::KeyEvent const&)> const& pred,
                std::function<void(ase::KeyEvent const&)> const& action,
                ase::KeyEvent const& event)
                -> InputResult
            {
                if (!pred(event))
                    return InputResult::unhandled;

                action(event);
                return InputResult::handled;
            };

            return makeWidgetTransformerResult(std::forward<TWidget>(widget)
                | detail::onKeyEvent(signal::mapFunction(std::move(f),
                            btl::clone(*predicate_), btl::clone(*action_)))
                );
        }

        inline OnKeyEvent acceptIf(
                signal::Convert<std::function<bool(ase::KeyEvent const&)>>
                predicate) &&
        {
            *predicate_ = signal::mapFunction([](
                    std::function<bool(ase::KeyEvent const&)> const& pred1,
                    std::function<bool(ase::KeyEvent const&)> const& pred2,
                    ase::KeyEvent const& e)
                {
                    return pred1(e) || pred2(e);
                }, std::move(*predicate_), std::move(predicate));

            return std::move(*this);
        }

        inline OnKeyEvent acceptIf(
                std::function<bool(ase::KeyEvent const&)> pred) &&
        {
            return std::move(*this)
                .acceptIf(signal::constant(std::move(pred)));
        }

        inline OnKeyEvent acceptIfNot(
                signal::Convert<std::function<bool(ase::KeyEvent const&)>>
                predicate) &&
        {
            auto inverse = [](std::function<bool(KeyEvent const&)> pred,
                    KeyEvent const& e)
            {
                return !pred(e);
            };

            return std::move(*this)
                .acceptIf(signal::mapFunction(std::move(inverse),
                            std::move(predicate)));
        }

        inline OnKeyEvent acceptIfNot(
                std::function<bool(ase::KeyEvent const&)> pred) &&
        {
            return std::move(*this)
                .acceptIf(signal::constant([pred](KeyEvent const& e)
                            { return !pred(e); }));
        }

        inline OnKeyEvent action(
                AnySignal<std::function<void(ase::KeyEvent const&)>> action) &&
        {
            *action_ = signal::mapFunction([](
                        std::function<void(ase::KeyEvent const&)> action1,
                        std::function<void(ase::KeyEvent const&)> action2,
                        ase::KeyEvent const& e)
                    {
                        action1(e);
                        action2(e);
                    }, std::move(*action_), std::move(action));

            return std::move(*this);
        }

        inline OnKeyEvent action(
                std::function<void(ase::KeyEvent const&)> action) &&
        {
            return std::move(*this)
                .action(signal::constant(std::move(action)));
        }

        template <typename TStreamHandle>
        inline auto send(TStreamHandle&& handle) &&
            -> decltype(std::declval<OnKeyEvent>()
                    .action(send(std::forward<TStreamHandle>(handle))))
        {
            return std::move(*this)
                .action(send(std::forward<TStreamHandle>(handle)));
        }

    private:
        btl::CloneOnCopy<signal::Convert<
            std::function<bool(ase::KeyEvent const&)>
            >> predicate_;

        btl::CloneOnCopy<signal::Convert<
            std::function<void(ase::KeyEvent const&)>
            >> action_;
    };

    inline OnKeyEvent onKeyEvent()
    {
        return {
            signal::constant([](ase::KeyEvent const&) { return false; }),
            signal::constant([](ase::KeyEvent const&) {})
        };
    }

    inline bool isNavigationKey(KeyEvent const& e)
    {
        if (!(e.getModifiers() & (uint32_t)ase::KeyModifier::Alt))
            return false;

        if (e.getKey() == ase::KeyCode::j
                || e.getKey() == ase::KeyCode::k
                || e.getKey() == ase::KeyCode::h
                || e.getKey() == ase::KeyCode::l)
            return true;

        return false;
    }

    template <typename TAction>
    auto onKeyEvent(TAction&& action)
        -> decltype(onKeyEvent().action(std::forward<TAction>(action)))
    {
        return onKeyEvent()
            .acceptIfNot(&isNavigationKey)
            .action(std::forward<TAction>(action));
    }
} // reactive::widget

