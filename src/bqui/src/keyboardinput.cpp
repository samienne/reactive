#include "bqui/keyboardinput.h"

#include "debug.h"

namespace bqui
{

KeyboardInput::KeyboardInput(avg::Obb obb) :
    obb_(std::move(obb))
{
}

KeyboardInput::KeyboardInput(
        avg::Obb obb,
        std::optional<FocusHandle> focusHandle,
        std::optional<KeyHandler> keyHandler,
        std::optional<TextHandler> textHandler,
        bool requestFocus,
        bool hasFocus) :
    obb_(std::move(obb)),
    focusHandle_(std::move(focusHandle)),
    keyHandler_(std::move(keyHandler)),
    textHandler_(std::move(textHandler)),
    requestFocus_(requestFocus),
    hasFocus_(hasFocus)
{
}

KeyboardInput KeyboardInput::requestFocus(bool focus) &&
{
    requestFocus_ = focus;
    return std::move(*this);
}

KeyboardInput KeyboardInput::setFocus(bool focus) &&
{
    hasFocus_ = focus;
    return std::move(*this);
}

KeyboardInput KeyboardInput::onKeyEvent(KeyHandler handler) &&
{
    keyHandler_ = std::make_optional(std::move(handler));
    return std::move(*this);
}

KeyboardInput KeyboardInput::onTextEvent(TextHandler handler) &&
{
    textHandler_ = std::make_optional(std::move(handler));
    return std::move(*this);
}

KeyboardInput KeyboardInput::setFocusHandle(FocusHandle handle) &&
{
    focusHandle_ = std::make_optional(std::move(handle));
    return std::move(*this);
}

KeyboardInput KeyboardInput::setFocusable(bool focusable) &&
{
    focusable_ = focusable;
    return std::move(*this);
}

KeyboardInput KeyboardInput::setFocusHandle(std::optional<FocusHandle> handle) &&
{
    focusHandle_ = std::move(handle);
    return std::move(*this);
}

KeyboardInput KeyboardInput::transform(avg::Transform const& t) &&
{
    obb_ = t * obb_;
    return std::move(*this);
}

KeyboardInput KeyboardInput::setObb(avg::Obb obb) &&
{
    obb_ = obb;
    return std::move(*this);
}

bool KeyboardInput::getRequestFocus() const
{
    return requestFocus_;
}

bool KeyboardInput::hasFocus() const
{
    return hasFocus_;
}

bool KeyboardInput::isFocusable() const
{
    return focusable_;
}

avg::Obb const& KeyboardInput::getObb() const
{
    return obb_;
}

std::optional<KeyboardInput::FocusHandle> const& KeyboardInput::getFocusHandle() const
{
    return focusHandle_;
}

std::optional<KeyboardInput::KeyHandler> const& KeyboardInput::getKeyHandler() const
{
    return keyHandler_;
}

std::optional<KeyboardInput::TextHandler> const& KeyboardInput::getTextHandler() const
{
    return textHandler_;
}

}

