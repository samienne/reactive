#include "genericwindow.h"

#include <iostream>

namespace ase
{

GenericWindow::GenericWindow(Vector2i size, float scalingFactor) :
    size_(size),
    scalingFactor_(scalingFactor)
{
}

Vector2i GenericWindow::getSize() const
{
    return size_;
}

Vector2i GenericWindow::getScaledSize() const
{
    return Vector2i(
            getWidth() * getScalingFactor(),
            getWidth() * getScalingFactor()
            );
}

void GenericWindow::setScalingFactor(float factor)
{
    if (scalingFactor_ == factor)
        return;

    scalingFactor_ = factor;
    if (resizeCallback_)
        resizeCallback_();
}

float GenericWindow::getScalingFactor() const
{
    return scalingFactor_;
}

float GenericWindow::getWidth() const
{
    return static_cast<float>(size_[0]);
}

float GenericWindow::getHeight() const
{
    return static_cast<float>(size_[1]);
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
    assert(pointerIndex <= 15);
    buttonPressedState_[buttonIndex - 1] = buttonState == ButtonState::down ;

    if (buttonState == ButtonState::down)
        buttonDownPos_[buttonIndex - 1] = pos;

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

    if (dragCallback_)
    {
        for (int i = 0; i < 15; ++i)
        {
            if (buttonPressedState_[i])
            {
                PointerDragEvent e{
                    i, pos, rel, buttonDownPos_[i], hover_
                };

                dragCallback_(e);
            }
        }
    }
}

void GenericWindow::injectHoverEvent(unsigned int pointerIndex, Vector2f pos,
        bool state)
{
    hover_ = state;

    injectPointerMoveEvent(pointerIndex, pos);

    if (hoverCallback_)
        hoverCallback_(HoverEvent{ state, state });
}

void GenericWindow::injectKeyEvent(KeyState keyState, KeyCode keyCode,
        uint32_t modifiers, std::string text)
{
    if (!keyCallback_)
        return;

    keyCallback_(KeyEvent(keyState, keyCode, modifiers, std::move(text)));
}

void GenericWindow::injectTextEvent(std::string text)
{
    if (!textCallback_)
        return;

    textCallback_(TextEvent(std::move(text)));
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

void GenericWindow::setDragCallback(
        std::function<void(PointerDragEvent const&)> cb)
{
    dragCallback_ = std::move(cb);
}

void GenericWindow::setKeyCallback(std::function<void(KeyEvent const&)> cb)
{
    keyCallback_ = std::move(cb);
}

void GenericWindow::setHoverCallback(std::function<void(HoverEvent const&)> cb)
{
    hoverCallback_ = std::move(cb);
}

void GenericWindow::setTextCallback(std::function<void(TextEvent const&)> cb)
{
    textCallback_ = std::move(cb);
}

} // namespace ase

