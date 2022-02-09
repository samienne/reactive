#include "widget/instance.h"

namespace reactive::widget
{

Instance::Instance(avg::RenderTree renderTree,
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

avg::RenderTree const& Instance::getRenderTree() const
{
    return renderTree_;
}

std::vector<InputArea> const& Instance::getInputAreas() const
{
    return inputAreas_;
}

avg::Obb const& Instance::getObb() const
{
    return obb_;
}

avg::Vector2f Instance::getSize() const
{
    return obb_.getSize();
}

std::vector<KeyboardInput> const& Instance::getKeyboardInputs() const
{
    return keyboardInputs_;
}

Theme const& Instance::getTheme() const
{
    return theme_;
}

Instance Instance::setRenderTree(avg::RenderTree renderTree) &&
{
    return Instance(
            std::move(renderTree),
            std::move(inputAreas_),
            obb_,
            std::move(keyboardInputs_),
            std::move(theme_)
            );
}

Instance Instance::setInputAreas(std::vector<InputArea> inputAreas) &&
{
    return Instance(
            std::move(renderTree_),
            std::move(inputAreas),
            obb_,
            std::move(keyboardInputs_),
            std::move(theme_)
            );
}

Instance Instance::setObb(avg::Obb const& obb) &&
{
    return Instance(
            std::move(renderTree_),
            std::move(inputAreas_),
            obb,
            std::move(keyboardInputs_),
            std::move(theme_)
            );
}

Instance Instance::setKeyboardInputs(std::vector<KeyboardInput> keyboardInputs) &&
{
    return Instance(
            std::move(renderTree_),
            std::move(inputAreas_),
            obb_,
            std::move(keyboardInputs),
            std::move(theme_)
            );
}

Instance Instance::setTheme(Theme theme) &&
{
    return Instance(
            std::move(renderTree_),
            std::move(inputAreas_),
            obb_,
            std::move(keyboardInputs_),
            std::move(theme)
            );
}

Instance Instance::transform(avg::Transform const& t) &&
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

Instance Instance::transformR(avg::Transform const& t) &&
{
    return std::move(*this).transform(
            obb_.getTransform() * t * obb_.getTransform().inverse()
            );
}

Instance Instance::clone() const
{
    return *this;
}
} // namespace reactive::widget

