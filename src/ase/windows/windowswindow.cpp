#include "windowswindow.h"

#include "dummyframebuffer.h"

#include <memory>

namespace ase
{

WindowsWindow::WindowsWindow() :
    defaultFramebuffer_(std::make_shared<DummyFramebuffer>())
{
}

void WindowsWindow::setVisible(bool value)
{
    visible_ = value;
}

bool WindowsWindow::isVisible() const
{
    return visible_;
}

void WindowsWindow::setTitle(std::string&& title)
{
    title_ = title;
}

std::string const& WindowsWindow::getTitle() const
{
    return title_;
}

Vector2i WindowsWindow::getSize() const
{
    return Vector2i(0, 0);
}

float WindowsWindow::getScalingFactor() const
{
    return 1.0f;
}

Framebuffer& WindowsWindow::getDefaultFramebuffer()
{
    return defaultFramebuffer_;
}

void WindowsWindow::clear()
{
}

void WindowsWindow::setCloseCallback(std::function<void()> /*func*/)
{
}

void WindowsWindow::setResizeCallback(std::function<void()> /*func*/)
{
}

void WindowsWindow::setRedrawCallback(std::function<void()> /*func*/)
{
}

void WindowsWindow::setButtonCallback(
        std::function<void(PointerButtonEvent const&)> /*cb*/)
{
}

void WindowsWindow::setPointerCallback(
        std::function<void(PointerMoveEvent const&)> /*cb*/)
{
}

void WindowsWindow::setDragCallback(
        std::function<void(PointerDragEvent const&)> /*cb*/)
{
}

void WindowsWindow::setKeyCallback(std::function<void(KeyEvent const&)> /*cb*/)
{
}

void WindowsWindow::setHoverCallback(std::function<void(HoverEvent const&)> /*cb*/)
{
}

} // namespace ase

