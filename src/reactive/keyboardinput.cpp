#include "keyboardinput.h"

#include "debug.h"

namespace reactive
{

KeyboardInput::KeyboardInput(avg::Obb obb) :
    obb_(std::move(obb))
{
}

KeyboardInput::KeyboardInput(
        avg::Obb obb,
        btl::option<FocusHandle> focusHandle,
        btl::option<Handler> handler,
        bool requestFocus,
        bool hasFocus) :
    obb_(std::move(obb)),
    focusHandle_(std::move(focusHandle)),
    handler_(std::move(handler)),
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

KeyboardInput KeyboardInput::onKeyEvent(Handler handler) &&
{
    handler_ = btl::just(std::move(handler));
    return std::move(*this);
}

KeyboardInput KeyboardInput::setFocusHandle(FocusHandle handle) &&
{
    focusHandle_ = btl::just(std::move(handle));
    return std::move(*this);
}

KeyboardInput KeyboardInput::setFocusable(bool focusable) &&
{
    focusable_ = focusable;
    return std::move(*this);
}

KeyboardInput KeyboardInput::setFocusHandle(btl::option<FocusHandle> handle) &&
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

btl::option<KeyboardInput::FocusHandle> const& KeyboardInput::getFocusHandle() const
{
    return focusHandle_;
}

btl::option<KeyboardInput::Handler> const& KeyboardInput::getHandler() const
{
    return handler_;
}

} // namespace

