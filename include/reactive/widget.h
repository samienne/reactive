#pragma once

#include "widget/theme.h"

#include "keyboardinput.h"
#include "inputarea.h"

#include "signal/map.h"
#include "signal/share.h"
#include "signal/signal.h"

#include <avg/rendertree.h>
#include <avg/obb.h>
#include <avg/transform.h>

#include <btl/tupleforeach.h>
#include <btl/option.h>

namespace reactive
{
    class InputArea;

    class Widget
    {
    public:
        Widget(avg::RenderTree renderTree,
                std::vector<InputArea> inputAreas,
                avg::Obb const& obb,
                std::vector<KeyboardInput> keyboardInputs,
                widget::Theme theme
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

        widget::Theme const& getTheme() const
        {
            return theme_;
        }

        Widget setRenderTree(avg::RenderTree renderTree) &&
        {
            return Widget(
                    std::move(renderTree),
                    std::move(inputAreas_),
                    obb_,
                    std::move(keyboardInputs_),
                    std::move(theme_)
                    );
        }

        Widget setInputAreas(std::vector<InputArea> inputAreas) &&
        {
            return Widget(
                    std::move(renderTree_),
                    std::move(inputAreas),
                    obb_,
                    std::move(keyboardInputs_),
                    std::move(theme_)
                    );
        }

        Widget setObb(avg::Obb const& obb) &&
        {
            return Widget(
                    std::move(renderTree_),
                    std::move(inputAreas_),
                    obb,
                    std::move(keyboardInputs_),
                    std::move(theme_)
                    );
        }

        Widget setKeyboardInputs(std::vector<KeyboardInput> keyboardInputs) &&
        {
            return Widget(
                    std::move(renderTree_),
                    std::move(inputAreas_),
                    obb_,
                    std::move(keyboardInputs),
                    std::move(theme_)
                    );
        }

        Widget setTheme(widget::Theme theme) &&
        {
            return Widget(
                    std::move(renderTree_),
                    std::move(inputAreas_),
                    obb_,
                    std::move(keyboardInputs_),
                    std::move(theme)
                    );
        }

        Widget transform(avg::Transform const& t) &&
        {
            for (auto&& a : inputAreas_)
                a = std::move(a).transform(t);

            for (auto&& k : keyboardInputs_)
                k = std::move(k).transform(t);

            return Widget(
                    std::move(renderTree_).transform(t),
                    std::move(inputAreas_),
                    t * obb_,
                    std::move(keyboardInputs_),
                    std::move(theme_)
                    );
        }

        Widget transformR(avg::Transform const& t) &&
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

        Widget clone() const
        {
            return *this;
        }

    private:
        avg::RenderTree renderTree_;
        std::vector<InputArea> inputAreas_;
        avg::Obb obb_;
        std::vector<KeyboardInput> keyboardInputs_;
        widget::Theme theme_;
    };

    template <typename T>
    auto makeWidget(Signal<T, avg::Vector2f> size)
    {
        auto focus = signal::input(false);

        return signal::map([focusHandle=std::move(focus.handle)]
                (avg::Vector2f size, bool focused)
                {
                    return Widget(
                            avg::RenderTree(),
                            {},
                            avg::Obb(size),
                            {
                                KeyboardInput(avg::Obb(size))
                                    .setFocus(focused)
                                    .setFocusHandle(focusHandle)
                            },
                            widget::Theme()
                            );
                },
                std::move(size),
                std::move(focus.signal)
                );
    }

} // reactive

