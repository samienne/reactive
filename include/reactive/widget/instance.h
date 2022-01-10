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
    class Instance
    {
    public:
        Instance(avg::RenderTree renderTree,
                std::vector<InputArea> inputAreas,
                avg::Obb const& obb,
                std::vector<KeyboardInput> keyboardInputs,
                Theme theme
              ) :
            renderTree_(std::move(renderTree)),
            inputAreas_(std::move(inputAreas)),
            obb_(obb),
            keyboardInputs_(std::move(keyboardInputs)),
            theme_(std::move(theme))
        {
        }

        avg::RenderTree const& getRenderTree() const
        {
            return renderTree_;
        }

        std::vector<InputArea> const& getInputAreas() const
        {
            return inputAreas_;
        }

        avg::Obb const& getObb() const
        {
            return obb_;
        }

        avg::Vector2f getSize() const
        {
            return obb_.getSize();
        }

        std::vector<KeyboardInput> const& getKeyboardInputs() const
        {
            return keyboardInputs_;
        }

        Theme const& getTheme() const
        {
            return theme_;
        }

        Instance setRenderTree(avg::RenderTree renderTree) &&
        {
            return Instance(
                    std::move(renderTree),
                    std::move(inputAreas_),
                    obb_,
                    std::move(keyboardInputs_),
                    std::move(theme_)
                    );
        }

        Instance setInputAreas(std::vector<InputArea> inputAreas) &&
        {
            return Instance(
                    std::move(renderTree_),
                    std::move(inputAreas),
                    obb_,
                    std::move(keyboardInputs_),
                    std::move(theme_)
                    );
        }

        Instance setObb(avg::Obb const& obb) &&
        {
            return Instance(
                    std::move(renderTree_),
                    std::move(inputAreas_),
                    obb,
                    std::move(keyboardInputs_),
                    std::move(theme_)
                    );
        }

        Instance setKeyboardInputs(std::vector<KeyboardInput> keyboardInputs) &&
        {
            return Instance(
                    std::move(renderTree_),
                    std::move(inputAreas_),
                    obb_,
                    std::move(keyboardInputs),
                    std::move(theme_)
                    );
        }

        Instance setTheme(Theme theme) &&
        {
            return Instance(
                    std::move(renderTree_),
                    std::move(inputAreas_),
                    obb_,
                    std::move(keyboardInputs_),
                    std::move(theme)
                    );
        }

        Instance transform(avg::Transform const& t) &&
        {
            for (auto&& a : inputAreas_)
                a = std::move(a).transform(t);

            for (auto&& k : keyboardInputs_)
                k = std::move(k).transform(t);

            return Instance(
                    std::move(renderTree_).transform(t),
                    std::move(inputAreas_),
                    t * obb_,
                    std::move(keyboardInputs_),
                    std::move(theme_)
                    );
        }

        Instance transformR(avg::Transform const& t) &&
        {
            return std::move(*this).transform(
                    obb_.getTransform() * t * obb_.getTransform().inverse()
                    );
        }

        template <typename TWidgetTransform>
        auto operator|(TWidgetTransform&& t) &&
        -> decltype(
            std::forward<TWidgetTransform>(t)(std::move(*this)).first
            )
        {
            return std::forward<TWidgetTransform>(t)(std::move(*this)).first;
        }

        Instance clone() const
        {
            return *this;
        }

    private:
        avg::RenderTree renderTree_;
        std::vector<InputArea> inputAreas_;
        avg::Obb obb_;
        std::vector<KeyboardInput> keyboardInputs_;
        Theme theme_;
    };

    template <typename T>
    auto makeWidget(Signal<T, avg::Vector2f> size)
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

