#include "dummywindow.h"

#include "dummyframebuffer.h"

#include <memory>

namespace ase
{

DummyWindow::DummyWindow(Vector2i size) :
    genericWindow_(size, 1.0f),
    defaultFramebuffer_(std::make_shared<DummyFramebuffer>())
{
}

void DummyWindow::setVisible(bool value)
{
    visible_ = value;
}

bool DummyWindow::isVisible() const
{
    return visible_;
}

void DummyWindow::setTitle(std::string&& title)
{
    genericWindow_.setTitle(std::move(title));
}

std::string const& DummyWindow::getTitle() const
{
    return genericWindow_.getTitle();
}

Vector2i DummyWindow::getSize() const
{
    return genericWindow_.getSize();
}

float DummyWindow::getScalingFactor() const
{
    return genericWindow_.getScalingFactor();
}

Framebuffer& DummyWindow::getDefaultFramebuffer()
{
    return defaultFramebuffer_;
}

void DummyWindow::requestFrame()
{
    genericWindow_.requestFrame();
}

void DummyWindow::setFrameCallback(
        std::function<std::optional<std::chrono::microseconds>(
            Frame const&)> cb)
{
    genericWindow_.setFrameCallback(std::move(cb));
}

void DummyWindow::setCloseCallback(std::function<void()> func)
{
    genericWindow_.setCloseCallback(std::move(func));
}

void DummyWindow::setResizeCallback(std::function<void()> func)
{
    genericWindow_.setResizeCallback(std::move(func));
}

void DummyWindow::setButtonCallback(
        std::function<void(PointerButtonEvent const&)> cb)
{
    genericWindow_.setButtonCallback(std::move(cb));
}

void DummyWindow::setPointerCallback(
        std::function<void(PointerMoveEvent const&)> cb)
{
    genericWindow_.setPointerCallback(std::move(cb));
}

void DummyWindow::setDragCallback(
        std::function<void(PointerDragEvent const&)> cb)
{
    genericWindow_.setDragCallback(std::move(cb));
}

void DummyWindow::setKeyCallback(std::function<void(KeyEvent const&)> cb)
{
    genericWindow_.setKeyCallback(std::move(cb));
}

void DummyWindow::setHoverCallback(std::function<void(HoverEvent const&)> cb)
{
    genericWindow_.setHoverCallback(std::move(cb));
}

void DummyWindow::setTextCallback(std::function<void(TextEvent const&)> cb)
{
    genericWindow_.setTextCallback(std::move(cb));
}

void DummyWindow::injectPointerButtonEvent(unsigned int pointerIndex,
        unsigned int buttonIndex, Vector2f pos, ButtonState buttonState)
{
    genericWindow_.injectPointerButtonEvent(pointerIndex, buttonIndex, pos,
            buttonState);
}

void DummyWindow::injectPointerMoveEvent(unsigned int pointerIndex,
        Vector2f pos)
{
    genericWindow_.injectPointerMoveEvent(pointerIndex, pos);
}

void DummyWindow::injectHoverEvent(unsigned int pointerIndex, Vector2f pos,
        bool state)
{
    genericWindow_.injectHoverEvent(pointerIndex, pos, state);
}

void DummyWindow::injectKeyEvent(KeyState keyState, KeyCode keyCode,
        uint32_t modifiers, std::string text)
{
    genericWindow_.injectKeyEvent(keyState, keyCode, modifiers,
            std::move(text));
}

void DummyWindow::injectTextEvent(std::string text)
{
    genericWindow_.injectTextEvent(std::move(text));
}

bool DummyWindow::needsRedraw() const
{
    return genericWindow_.needsRedraw();
}

std::optional<std::chrono::microseconds> DummyWindow::frame(Frame const& frame)
{
    return genericWindow_.frame(frame);
}

} // namespace ase
