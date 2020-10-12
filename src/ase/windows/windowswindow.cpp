#include "windowswindow.h"

#include "dummyframebuffer.h"

#include <memory>

#include <windows.h>

namespace ase
{

WindowsWindow::WindowsWindow(Vector2i size) :
    defaultFramebuffer_(std::make_shared<DummyFramebuffer>())
{
    HINSTANCE hInst = GetModuleHandle(NULL);

    hwnd_ = CreateWindowEx(
            0,
            "MainWindowClass",
            "Basic window",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            size[0], size[1],
            NULL, // parent
            NULL, // Menu
            hInst,
            NULL // Additional application data
            );

    if (hwnd_ == 0)
        throw std::runtime_error("Unable to create window");
}

void WindowsWindow::setVisible(bool value)
{
    if (visible_ == value)
        return;

    visible_ = value;

    if (visible_)
        ShowWindow(hwnd_, SW_SHOWNORMAL);
    else
        ShowWindow(hwnd_, SW_HIDE);
}

bool WindowsWindow::isVisible() const
{
    return visible_;
}

void WindowsWindow::setTitle(std::string&& title)
{
    title_ = title;

    SetWindowText(hwnd_, title_.c_str());
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

