#pragma once

#include "setkeyboardinputs.h"
#include "bindkeyboardinputs.h"
#include "bindobb.h"
#include "widgettransformer.h"

#include <reactive/signal/tee.h>
#include <reactive/signal/inputhandle.h>

#include <ase/vector.h>

namespace reactive::widget
{
    inline auto trackSize(signal::InputHandle<ase::Vector2f> handle)
        //-> FactoryMap
    {
        auto f = [handle = std::move(handle)](auto widget) mutable
            // -> Widget
        {
            auto obb = signal::tee(
                    widget.getObb(),
                    std::mem_fn(&avg::Obb::getSize),
                    std::move(handle)
                    );

            return widget::makeWidgetTransformerResult(std::move(widget)
                .setObb(std::move(obb))
                | widget::makeWidgetTransformer()
                .compose(bindObb(), grabKeyboardInputs())
                .bind([](auto obb, auto inputs)
                {
                    auto newInputs = signal::map(
                        [](avg::Obb, std::vector<KeyboardInput> inputs)
                        {
                            // TODO: Remove this hack. This is needed to keep
                            // the obb signal in around.
                            return inputs;
                        },
                        std::move(obb),
                        std::move(inputs)
                        );

                    return widget::setKeyboardInputs(std::move(newInputs));
                }));
        };

        return widget::makeWidgetTransformer(std::move(f));
    }

} // namespace reactive::widget

