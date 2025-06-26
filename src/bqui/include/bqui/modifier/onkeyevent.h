#pragma once

#include "instancemodifier.h"
#include "widgetmodifier.h"

#include "bqui/inputresult.h"

#include <bq/signal/signal.h>

#include <ase/keyevent.h>

namespace bqui::modifier
{
    namespace detail
    {
        template <typename T, typename U>
        auto onKeyEvent(bq::signal::Signal<T, U> handler)
        {
            return makeInstanceModifier([](widget::Instance instance, auto handler)
                {
                    auto inputs = instance.getKeyboardInputs();

                    for (auto&& input : inputs)
                        input = std::move(input)
                            .onKeyEvent(handler);

                    return std::move(instance)
                        .setKeyboardInputs(std::move(inputs))
                        ;
                },
                std::move(handler)
                );
        }

        inline auto onKeyEvent(KeyboardInput::KeyHandler handler)
        {
            return onKeyEvent(bq::signal::constant(std::move(handler)));
        }
    } // namespace detail

    class BQUI_EXPORT OnKeyEvent
    {
    public:
        OnKeyEvent(
                bq::signal::AnySignal<std::function<bool(ase::KeyEvent const&)>> predicate,
                bq::signal::AnySignal<std::function<void(ase::KeyEvent const&)>> action);

        template <typename T>
        auto operator()(bq::signal::Signal<T, widget::Instance> instance) const
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

            return std::move(instance)
                | detail::onKeyEvent(
                        merge(predicate_, action_).bindToFunction(f)
                        )
                ;
        }

        OnKeyEvent acceptIf(
                bq::signal::AnySignal<std::function<bool(ase::KeyEvent const&)>>
                predicate) &&;

        OnKeyEvent acceptIf(
                std::function<bool(ase::KeyEvent const&)> pred) &&;

        OnKeyEvent acceptIfNot(
                bq::signal::AnySignal<std::function<bool(ase::KeyEvent const&)>>
                predicate) &&;

        OnKeyEvent acceptIfNot(
                std::function<bool(ase::KeyEvent const&)> pred) &&;

        OnKeyEvent action(
                bq::signal::AnySignal<std::function<void(ase::KeyEvent const&)>> action) &&;

        OnKeyEvent action(
                std::function<void(ase::KeyEvent const&)> action) &&;

        template <typename TStreamHandle>
        auto send(TStreamHandle&& handle) &&
            -> decltype(std::declval<OnKeyEvent>()
                    .action(send(std::forward<TStreamHandle>(handle))))
        {
            return std::move(*this)
                .action(send(std::forward<TStreamHandle>(handle)));
        }

    private:
        bq::signal::AnySignal<std::function<bool(ase::KeyEvent const&)>> predicate_;
        bq::signal::AnySignal<std::function<void(ase::KeyEvent const&)>> action_;
    };

    BQUI_EXPORT OnKeyEvent onKeyEvent();

    BQUI_EXPORT bool isNavigationKey(KeyEvent const& e);

    BQUI_EXPORT AnyWidgetModifier onKeyEvent(
            std::function<InputResult(ase::KeyEvent const&)> cb);

    BQUI_EXPORT AnyWidgetModifier onKeyEvent(
            bq::signal::AnySignal<std::function<InputResult(ase::KeyEvent const&)>> cb);

}

