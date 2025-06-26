#pragma once

#include "bqui/theme.h"
#include "bqui/keyboardinput.h"
#include "bqui/inputarea.h"

#include <bq/signal/signal.h>

#include <avg/rendertree.h>
#include <avg/obb.h>
#include <avg/transform.h>

#include <btl/tupleforeach.h>

namespace bqui::widget
{
    class BQUI_EXPORT Instance
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
    auto makeInstance(bq::signal::Signal<T, avg::Vector2f> size)
    {
        auto focus = bq::signal::makeInput(false);

        return merge(std::move(size), std::move(focus.signal))
            .map([focusHandle=std::move(focus.handle)]
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
                });
    }

} // bqui::widget

