#include "reactive/widget/focusgroup.h"

#include "reactive/widget/onkeyevent.h"
#include "reactive/widget/setkeyboardinputs.h"
#include "reactive/widget/widgetmodifier.h"

#include "reactive/widget.h"

#include "reactive/signal/foldp.h"
#include "reactive/stream/pipe.h"
#include "reactive/stream/collect.h"

#include "debug.h"

#include <algorithm>
#include <unordered_map>
#include <functional>

namespace reactive::widget
{

namespace
{

struct FocusGroupState
{
    std::vector<KeyboardInput> inputs;
    size_t focusIndex = 0;
};

enum class Direction
{
    none,
    left,
    right,
    up,
    down,
    next,
    previous
};

btl::option<size_t> getClosestInDirection(
        std::vector<KeyboardInput> const& inputs, size_t currentIndex,
        Direction dir)
{
    if (inputs.empty())
        return btl::none;

    auto const& current = inputs[currentIndex];

    bool h1 = dir == Direction::right || dir == Direction::up;
    bool h2 = dir == Direction::right || dir == Direction::down;
    const float sqrt2 = std::sqrt(2.0);

    const ase::Vector2f n1(sqrt2, sqrt2);
    const ase::Vector2f n2(sqrt2, -sqrt2);
    const ase::Vector2f center = current.getObb().getCenter();
    const float d1 = n1.dot(center);
    const float d2 = n2.dot(center);

    float closestDistance = 0.0f;
    btl::option<size_t> result(btl::none);

    for (size_t i = 0; i < inputs.size(); ++i)
    {
        if (i == currentIndex)
            continue;

        auto&& input = inputs[i];

        auto inputCenter = input.getObb().getCenter();
        bool b1 = n1.dot(inputCenter) > d1;
        bool b2 = n2.dot(inputCenter) > d2;

        if (b1 == h1 && b2 == h2)
        {
            ase::Vector2f v = inputCenter - center;
            float inputDistance = v.x() * v.x() + v.y() * v.y();

            if (!result.valid() || inputDistance < closestDistance)
            {
                closestDistance = inputDistance;
                result = btl::just(i);
            }
        }
    }

    return result;
}

size_t getFocusIndex(FocusGroupState const& oldState,
        std::vector<KeyboardInput> const& inputs)
{
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        if (inputs[i].hasFocus())
            return i;
    }

    if (oldState.focusIndex >= oldState.inputs.size())
        return 0;

    auto i = std::find_if(inputs.begin(), inputs.end(),
            [&](KeyboardInput const& input)
            {
                return input.getFocusHandle()
                    == oldState.inputs[oldState.focusIndex].getFocusHandle();
            });

    if (i == inputs.end())
        return oldState.focusIndex;

    return std::distance(inputs.begin(), i);
}

const std::unordered_map<ase::KeyCode, Direction> directions =
{
    { ase::KeyCode::h, Direction::left },
    { ase::KeyCode::l, Direction::right },
    { ase::KeyCode::j, Direction::down },
    { ase::KeyCode::k, Direction::up },
    { ase::KeyCode::tab, Direction::next },
};

Direction getNavigationDirection(KeyEvent const& e)
{
    auto i = directions.find(e.getKey());
    if (i != directions.end())
        return i->second;

    return Direction::none;
}

bool canNavigate(FocusGroupState const& state, KeyEvent const& e)
{
    auto dir = getNavigationDirection(e);
    switch (dir)
    {
    case Direction::left:
    case Direction::right:
    case Direction::up:
    case Direction::down:
        return getClosestInDirection(state.inputs, state.focusIndex,
                dir).valid();
    case Direction::previous:
        return state.focusIndex != 0;
    case Direction::next:
        return state.focusIndex < (state.inputs.size() - 1);
    default:
        return false;
    }
}

 auto makeKeyHandler(
         stream::Handle<KeyEvent> keyHandle,
         btl::option<KeyboardInput::KeyHandler> handler,
         FocusGroupState state)
    -> KeyboardInput::KeyHandler
{
    return [handler, keyHandle, state](KeyEvent const& e) mutable
        -> InputResult
    {
        if (handler.valid())
        {
            auto r = (*handler)(e);
            if (r == InputResult::handled)
                return InputResult::handled;
        }

        if (e.getState() == ase::KeyState::down
                && canNavigate(state, e))
        {
            keyHandle.push(e);
            return InputResult::handled;
        }

        return InputResult::unhandled;
    };
}

auto makeTextHandler(btl::option<KeyboardInput::TextHandler> handler)
{
    return [handler=std::move(handler)]
        (TextEvent const& e) mutable -> InputResult
    {
        if (!handler.valid())
            return InputResult::unhandled;

        return (*handler)(e);
    };
}


std::vector<KeyboardInput> mapStateToInputs(
    FocusGroupState const& state,
    stream::Handle<KeyEvent> keyHandle,
    avg::Obb obb
    )
{
    if (state.inputs.empty())
        return { KeyboardInput(std::move(obb)) };

    auto result = KeyboardInput(std::move(obb));

    bool requested = false;
    for (auto&& input : state.inputs)
    {
        if (!input.getRequestFocus())
            continue;

        requested = true;

        result = std::move(result)
            .requestFocus(true)
            .setFocusHandle(input.getFocusHandle())
            .onKeyEvent(makeKeyHandler(keyHandle, input.getKeyHandler(), state))
            .onTextEvent(makeTextHandler(input.getTextHandler()))
            ;

        break;
    }

    if (!requested)
    {
        auto&& input = state.inputs.at(state.focusIndex);
        result = std::move(result)
            .setFocusHandle(input.getFocusHandle())
            .onKeyEvent(makeKeyHandler(keyHandle, input.getKeyHandler(), state))
            .onTextEvent(makeTextHandler(input.getTextHandler()))
            ;
    }

    for (auto&& input : state.inputs)
    {
        if (input.hasFocus())
        {
            result = std::move(result)
                .setFocus(true);
            break;
        }
    }

    return { std::move(result).setFocusable(true) };
}

FocusGroupState step(FocusGroupState oldState,
        std::vector<KeyboardInput> inputs,
        std::vector<KeyEvent> const& events)
{
    FocusGroupState state;
    state.focusIndex = getFocusIndex(oldState, inputs);
    state.inputs = std::move(inputs);

    size_t index = state.focusIndex;
    for (auto&& event : events)
    {
        auto dir = getNavigationDirection(event);
        auto closest = getClosestInDirection(state.inputs, index, dir);

        if (closest.valid())
            index = *closest;
    }

    index = std::min(index, state.inputs.size() - 1);

    if (index != state.focusIndex)
    {
        state.inputs[index] = std::move(state.inputs[index])
            .requestFocus(true);
        state.focusIndex = index;
    }

    return state;
}

} // anonymous

WidgetTransformer<void> focusGroup()
{
    return makeSharedWidgetSignalModifier([](auto widget)
        {
            auto inputs = signal::map(&Widget::getKeyboardInputs, widget);
            auto obb = signal::map(&Widget::getObb, widget);

            auto keyStream = stream::pipe<KeyEvent>();

            auto state = signal::foldp(&step,
                    FocusGroupState(),
                    std::move(inputs),
                    stream::collect(std::move(keyStream.stream))
                    );

            auto newInputs = signal::map(
                    mapStateToInputs,
                    std::move(state),
                    signal::constant(keyStream.handle),
                    std::move(obb)
                    );

            return std::move(widget)
                | setKeyboardInputs(std::move(newInputs))
                ;
        });
}

} // reactive::widget

