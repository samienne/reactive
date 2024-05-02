#include "widget/ontextevent.h"

#include "widget/instancemodifier.h"

namespace reactive::widget
{

AnyWidgetModifier onTextEvent(signal2::AnySignal<KeyboardInput::TextHandler> handler)
{
    return makeWidgetModifier(
        makeInstanceModifier([](Instance instance, auto handler)
        {
            auto inputs = instance.getKeyboardInputs();
            for (auto&& input : inputs)
            {
                input = std::move(input)
                    .onTextEvent(handler);
            }

            return std::move(instance)
                .setKeyboardInputs(std::move(inputs))
                ;
        },
        std::move(handler)
        ));
}

AnyWidgetModifier onTextEvent(KeyboardInput::TextHandler handler)
{
    return onTextEvent(signal2::constant(std::move(handler)));
}

} // namespace reactive::widget

