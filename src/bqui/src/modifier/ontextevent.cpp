#include "bqui/modifier/ontextevent.h"

#include "bqui/modifier/instancemodifier.h"

namespace bqui::modifier
{

AnyWidgetModifier onTextEvent(bq::signal::AnySignal<KeyboardInput::TextHandler> handler)
{
    return makeWidgetModifier(
        makeInstanceModifier([](widget::Instance instance, auto handler)
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
    return onTextEvent(bq::signal::constant(std::move(handler)));
}

}

