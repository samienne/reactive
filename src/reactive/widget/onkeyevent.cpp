#include "widget/onkeyevent.h"

#include "widget/instancemodifier.h"
#include "widget/widget.h"

#include "reactive/inputresult.h"

#include <bq/signal/signal.h>

#include <ase/keyevent.h>

namespace reactive::widget
{

OnKeyEvent::OnKeyEvent(
        signal::AnySignal<std::function<bool(ase::KeyEvent const&)>> predicate,
        signal::AnySignal<std::function<void(ase::KeyEvent const&)>> action) :
    predicate_(std::move(predicate)),
    action_(std::move(action))
{
}

OnKeyEvent OnKeyEvent::acceptIf(
        signal::AnySignal<std::function<bool(ase::KeyEvent const&)>>
        predicate) &&
{
    predicate_ = merge(std::move(predicate_), std::move(predicate))
        .bindToFunction([](
            std::function<bool(ase::KeyEvent const&)> const& pred1,
            std::function<bool(ase::KeyEvent const&)> const& pred2,
            ase::KeyEvent const& e)
        {
            return pred1(e) || pred2(e);
        });

    return std::move(*this);
}

OnKeyEvent OnKeyEvent::acceptIf(
        std::function<bool(ase::KeyEvent const&)> pred) &&
{
    return std::move(*this)
        .acceptIf(signal::constant(std::move(pred)));
}

OnKeyEvent OnKeyEvent::acceptIfNot(
        signal::AnySignal<std::function<bool(ase::KeyEvent const&)>>
        predicate) &&
{
    auto inverse = [](std::function<bool(KeyEvent const&)> pred,
            KeyEvent const& e)
    {
        return !pred(e);
    };

    return std::move(*this)
        .acceptIf(std::move(predicate).bindToFunction(std::move(inverse)));
}

OnKeyEvent OnKeyEvent::acceptIfNot(
        std::function<bool(ase::KeyEvent const&)> pred) &&
{
    return std::move(*this)
        .acceptIf(signal::constant([pred](KeyEvent const& e)
                    { return !pred(e); }));
}

OnKeyEvent OnKeyEvent::action(
        signal::AnySignal<std::function<void(ase::KeyEvent const&)>> action) &&
{
    action_ = merge(std::move(action_), std::move(action))
        .bindToFunction([](
                std::function<void(ase::KeyEvent const&)> action1,
                std::function<void(ase::KeyEvent const&)> action2,
                ase::KeyEvent const& e)
            {
                action1(e);
                action2(e);
            });

    return std::move(*this);
}

OnKeyEvent OnKeyEvent::action(
        std::function<void(ase::KeyEvent const&)> action) &&
{
    return std::move(*this)
        .action(signal::constant(std::move(action)));
}


OnKeyEvent onKeyEvent()
{
    return {
        signal::constant([](ase::KeyEvent const&) { return false; }),
        signal::constant([](ase::KeyEvent const&) {})
    };
}

bool isNavigationKey(KeyEvent const& e)
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

AnyWidgetModifier onKeyEvent(std::function<InputResult(ase::KeyEvent const&)> cb)
{
    return makeWidgetModifier(makeInstanceSignalModifier(
            onKeyEvent()
            .acceptIfNot(&isNavigationKey)
            .action(std::move(cb))
            ));
}

AnyWidgetModifier onKeyEvent(signal::AnySignal<std::function<InputResult(
            ase::KeyEvent const&)>> cb)
{
    return makeWidgetModifier(makeInstanceSignalModifier(
            onKeyEvent()
            .acceptIfNot(&isNavigationKey)
            .action(std::move(cb))
            ));
}

} // reactive::widget

