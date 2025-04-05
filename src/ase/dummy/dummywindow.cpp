#include "dummywindow.h"

#include "dummyframebuffer.h"

#include <memory>

namespace ase
{

DummyWindow::DummyWindow() :
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
    title_ = title;
}

std::string const& DummyWindow::getTitle() const
{
    return title_;
}

Vector2i DummyWindow::getSize() const
{
    return Vector2i(0, 0);
}

float DummyWindow::getScalingFactor() const
{
    return 1.0f;
}

Framebuffer& DummyWindow::getDefaultFramebuffer()
{
    return defaultFramebuffer_;
}

void DummyWindow::requestFrame()
{
}

void DummyWindow::setFrameCallback(
        std::function<std::optional<std::chrono::microseconds>(
            Frame const&)>)
{
}

void DummyWindow::setCloseCallback(std::function<void()> /*func*/)
{
}

void DummyWindow::setResizeCallback(std::function<void()> /*func*/)
{
}

void DummyWindow::setButtonCallback(
        std::function<void(PointerButtonEvent const&)> /*cb*/)
{
}

void DummyWindow::setPointerCallback(
        std::function<void(PointerMoveEvent const&)> /*cb*/)
{
}

void DummyWindow::setDragCallback(
        std::function<void(PointerDragEvent const&)> /*cb*/)
{
}

void DummyWindow::setKeyCallback(std::function<void(KeyEvent const&)> /*cb*/)
{
}

void DummyWindow::setHoverCallback(std::function<void(HoverEvent const&)> /*cb*/)
{
}

void DummyWindow::setTextCallback(std::function<void(TextEvent const&)> /*cb*/)
{
}

} // namespace ase

