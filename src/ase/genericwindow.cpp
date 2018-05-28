#include "genericwindow.h"

#include <iostream>

namespace ase
{

GenericWindow::GenericWindow(Vector2i size) :
    size_(size)
{
}

Vector2i GenericWindow::getSize() const
{
    return size_;
}

float GenericWindow::getWidth() const
{
    return size_[0];
}

float GenericWindow::getHeight() const
{
    return size_[1];
}

std::string const& GenericWindow::getTitle() const
{
    return title_;
}

void GenericWindow::setTitle(std::string title)
{
    title_ = title;
}

void GenericWindow::notifyClose()
{
    if (closeCallback_)
        closeCallback_();
}

void GenericWindow::resize(Vector2i size)
{
    size_ = size;

    if (resizeCallback_)
        resizeCallback_();
}

void GenericWindow::notifyRedraw()
{
    if (redrawCallback_)
        redrawCallback_();
}

void GenericWindow::injectPointerButtonEvent(
        unsigned int pointerIndex,
        unsigned int buttonIndex,
        Vector2f pos,
        ButtonState buttonState)
{
    buttonPressedState_[buttonIndex - 1] = true;

    PointerButtonEvent event{
                pointerIndex, buttonIndex, buttonState, pos
    };

    if (buttonCallback_)
        buttonCallback_(event);
}

void GenericWindow::injectPointerMoveEvent(unsigned int /*pointerIndex*/,
        Vector2f pos)
{
    Vector2f rel(0.0f, 0.0f);

    if (pointerPos_.valid())
        rel = pos - *pointerPos_;

    pointerPos_ = btl::just(pos);

    PointerMoveEvent event{
        pos, rel, buttonPressedState_, hover_
    };

    if (pointerCallback_)
        pointerCallback_(event);
}

void GenericWindow::injectHoverEvent(unsigned int pointerIndex, Vector2f pos,
        bool state)
{
    hover_ = state;

    injectPointerMoveEvent(pointerIndex, pos);

    if (hoverCallback_)
        hoverCallback_(HoverEvent{ state });
}

void GenericWindow::injectKeyEvent(KeyState keyState, KeyCode keyCode,
        uint32_t modifiers, std::string text)
{
    if (!keyCallback_)
        return;

    keyCallback_(KeyEvent(keyState, keyCode, modifiers, std::move(text)));
}

void GenericWindow::setCloseCallback(std::function<void()> func)
{
    closeCallback_ = std::move(func);
}

void GenericWindow::setResizeCallback(std::function<void()> func)
{
    resizeCallback_ = std::move(func);
}

void GenericWindow::setRedrawCallback(std::function<void()> func)
{
    redrawCallback_ = std::move(func);
}

void GenericWindow::setButtonCallback(
        std::function<void(PointerButtonEvent const& e)> cb)
{
    buttonCallback_ = std::move(cb);
}

void GenericWindow::setPointerCallback(
        std::function<void(PointerMoveEvent const&)> cb)
{
    pointerCallback_ = std::move(cb);
}

void GenericWindow::setKeyCallback(std::function<void(KeyEvent const&)> cb)
{
    keyCallback_ = std::move(cb);
}

void GenericWindow::setHoverCallback(std::function<void(HoverEvent const&)> cb)
{
    hoverCallback_ = std::move(cb);
}
} // namespace ase

