#include "widget/trackfocus.h"

#include "widget/instancemodifier.h"
#include "widget/setkeyboardinputs.h"
#include "widget/instance.h"

#include <reactive/signal/tee.h>
#include <reactive/signal/inputhandle.h>

namespace reactive::widget
{

namespace
{
    bool anyHasFocus(std::vector<KeyboardInput> const& inputs)
    {
        for (auto&& input : inputs)
            if (input.hasFocus())
                return true;

        return false;
    }
} // anonymous namespace

AnyWidgetModifier trackFocus(signal::InputHandle<bool> const& handle)
{
    return makeWidgetModifier(
            makeSharedInstanceSignalModifier([](auto instance, auto handle)
            {
                auto input = signal::tee(
                        signal::map(&Instance::getKeyboardInputs, instance),
                        anyHasFocus,
                        std::move(handle)
                        );

                return std::move(instance)
                    | setKeyboardInputs(std::move(input))
                    ;
            },
            std::move(handle)
            ));
}
} // namespace reactive::widget
