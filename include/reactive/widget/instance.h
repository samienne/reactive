#pragma once

#include "theme.h"

#include "reactive/keyboardinput.h"
#include "reactive/inputarea.h"

#include "reactive/signal/map.h"
#include "reactive/signal/share.h"
#include "reactive/signal/signal.h"

#include <avg/rendertree.h>
#include <avg/obb.h>
#include <avg/transform.h>

#include <btl/tupleforeach.h>
#include <btl/option.h>

namespace reactive::widget
{
    class REACTIVE_EXPORT Instance
    {
    public:
        Instance(avg::RenderTree renderTree,
                std::vector<InputArea> inputAreas,
                avg::Obb const& obb,
                std::vector<KeyboardInput> keyboardInputs,
                Theme theme
              );

        avg::RenderTree const& getRenderTree() const;
        std::vector<InputArea> const& getInputAreas() const;
        avg::Obb const& getObb() const;
        avg::Vector2f getSize() const;
        std::vector<KeyboardInput> const& getKeyboardInputs() const;
        Theme const& getTheme() const;
        Instance setRenderTree(avg::RenderTree renderTree) &&;
        Instance setInputAreas(std::vector<InputArea> inputAreas) &&;
        Instance setObb(avg::Obb const& obb) &&;
        Instance setKeyboardInputs(std::vector<KeyboardInput> keyboardInputs) &&;
        Instance setTheme(Theme theme) &&;
        Instance transform(avg::Transform const& t) &&;
        Instance transformR(avg::Transform const& t) &&;
        Instance clone() const;

    private:
        avg::RenderTree renderTree_;
        std::vector<InputArea> inputAreas_;
        avg::Obb obb_;
        std::vector<KeyboardInput> keyboardInputs_;
        Theme theme_;
    };

    template <typename T>
    auto makeInstance(Signal<T, avg::Vector2f> size)
    {
        auto focus = signal::input(false);

        return signal::map([focusHandle=std::move(focus.handle)]
                (avg::Vector2f size, bool focused)
                {
                    return Instance(
                            avg::RenderTree(),
                            {},
                            avg::Obb(size),
                            {
                                KeyboardInput(avg::Obb(size))
                                    .setFocus(focused)
                                    .setFocusHandle(focusHandle)
                            },
                            Theme()
                            );
                },
                std::move(size),
                std::move(focus.signal)
                );
    }

} // reactive::widget

