#include "bqui/modifier/trackfocus.h"

#include "bqui/modifier/instancemodifier.h"
#include "bqui/modifier/setkeyboardinputs.h"

#include "bqui/widget/instance.h"

#include <bq/signal/signal.h>

namespace bqui::modifier
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

AnyWidgetModifier trackFocus(bq::signal::InputHandle<bool> const& handle)
{
    return makeWidgetModifier(
            makeSharedInstanceSignalModifier([](auto instance, auto handle)
            {
                auto input = instance.map(&widget::Instance::getKeyboardInputs)
                    .tee(std::move(handle), anyHasFocus);

                return std::move(instance)
                    | setKeyboardInputs(std::move(input))
                    ;
            },
            std::move(handle)
            ));
}
}
