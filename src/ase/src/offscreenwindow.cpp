#include "offscreenwindow.h"

#include "rendercontext.h"

namespace ase
{

OffscreenWindow::OffscreenWindow(RenderContext& context, Vector2i size) :
    genericWindow_(size, 1.0f),
    texture_(context.makeTexture(size, FORMAT_SRGBA)),
    depthbuffer_(context.makeRenderbuffer(size, FORMAT_DEPTH16)),
    framebuffer_(context.makeFramebuffer())
{
    framebuffer_.setColorTarget(0, texture_);
    framebuffer_.setDepthTarget(depthbuffer_);
}

void OffscreenWindow::setVisible(bool /*value*/)
{
}

bool OffscreenWindow::isVisible() const
{
    return false;
}

void OffscreenWindow::setTitle(std::string&& title)
{
    genericWindow_.setTitle(std::move(title));
}

std::string const& OffscreenWindow::getTitle() const
{
    return genericWindow_.getTitle();
}

Vector2i OffscreenWindow::getSize() const
{
    return genericWindow_.getSize();
}

float OffscreenWindow::getScalingFactor() const
{
    return genericWindow_.getScalingFactor();
}

Framebuffer& OffscreenWindow::getDefaultFramebuffer()
{
    return framebuffer_;
}

void OffscreenWindow::requestFrame()
{
    genericWindow_.requestFrame();
}

void OffscreenWindow::setFrameCallback(
        std::function<std::optional<std::chrono::microseconds>(
            Frame const&)> cb)
{
    genericWindow_.setFrameCallback(std::move(cb));
}

void OffscreenWindow::setCloseCallback(std::function<void()> func)
{
    genericWindow_.setCloseCallback(std::move(func));
}

void OffscreenWindow::setResizeCallback(std::function<void()> func)
{
    genericWindow_.setResizeCallback(std::move(func));
}

void OffscreenWindow::setButtonCallback(
        std::function<void(PointerButtonEvent const&)> cb)
{
    genericWindow_.setButtonCallback(std::move(cb));
}

void OffscreenWindow::setPointerCallback(
        std::function<void(PointerMoveEvent const&)> cb)
{
    genericWindow_.setPointerCallback(std::move(cb));
}

void OffscreenWindow::setDragCallback(
        std::function<void(PointerDragEvent const&)> cb)
{
    genericWindow_.setDragCallback(std::move(cb));
}

void OffscreenWindow::setKeyCallback(std::function<void(KeyEvent const&)> cb)
{
    genericWindow_.setKeyCallback(std::move(cb));
}

void OffscreenWindow::setHoverCallback(std::function<void(HoverEvent const&)> cb)
{
    genericWindow_.setHoverCallback(std::move(cb));
}

void OffscreenWindow::setTextCallback(std::function<void(TextEvent const&)> cb)
{
    genericWindow_.setTextCallback(std::move(cb));
}

void OffscreenWindow::injectPointerButtonEvent(unsigned int pointerIndex,
        unsigned int buttonIndex, Vector2f pos, ButtonState buttonState)
{
    genericWindow_.injectPointerButtonEvent(pointerIndex, buttonIndex, pos,
            buttonState);
}

void OffscreenWindow::injectPointerMoveEvent(unsigned int pointerIndex,
        Vector2f pos)
{
    genericWindow_.injectPointerMoveEvent(pointerIndex, pos);
}

void OffscreenWindow::injectHoverEvent(unsigned int pointerIndex, Vector2f pos,
        bool state)
{
    genericWindow_.injectHoverEvent(pointerIndex, pos, state);
}

void OffscreenWindow::injectKeyEvent(KeyState keyState, KeyCode keyCode,
        uint32_t modifiers, std::string text)
{
    genericWindow_.injectKeyEvent(keyState, keyCode, modifiers,
            std::move(text));
}

void OffscreenWindow::injectTextEvent(std::string text)
{
    genericWindow_.injectTextEvent(std::move(text));
}

bool OffscreenWindow::needsRedraw() const
{
    return genericWindow_.needsRedraw();
}

std::optional<std::chrono::microseconds> OffscreenWindow::frame(
        Frame const& frame)
{
    return genericWindow_.frame(frame);
}

} // namespace ase
