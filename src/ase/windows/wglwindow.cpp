#include "wglwindow.h"

#include "dummyframebuffer.h"

#include <memory>

#include <windows.h>

namespace ase
{

WglWindow::WglWindow(Vector2i size) :
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

void WglWindow::setVisible(bool value)
{
    if (visible_ == value)
        return;

    visible_ = value;

    if (visible_)
        ShowWindow(hwnd_, SW_SHOWNORMAL);
    else
        ShowWindow(hwnd_, SW_HIDE);
}

bool WglWindow::isVisible() const
{
    return visible_;
}

void WglWindow::setTitle(std::string&& title)
{
    title_ = title;

    SetWindowText(hwnd_, title_.c_str());
}

std::string const& WglWindow::getTitle() const
{
    return title_;
}

Vector2i WglWindow::getSize() const
{
    return Vector2i(0, 0);
}

float WglWindow::getScalingFactor() const
{
    return 1.0f;
}

Framebuffer& WglWindow::getDefaultFramebuffer()
{
    return defaultFramebuffer_;
}

void WglWindow::clear()
{
}

void WglWindow::setCloseCallback(std::function<void()> /*func*/)
{
}

void WglWindow::setResizeCallback(std::function<void()> /*func*/)
{
}

void WglWindow::setRedrawCallback(std::function<void()> /*func*/)
{
}

void WglWindow::setButtonCallback(
        std::function<void(PointerButtonEvent const&)> /*cb*/)
{
}

void WglWindow::setPointerCallback(
        std::function<void(PointerMoveEvent const&)> /*cb*/)
{
}

void WglWindow::setDragCallback(
        std::function<void(PointerDragEvent const&)> /*cb*/)
{
}

void WglWindow::setKeyCallback(std::function<void(KeyEvent const&)> /*cb*/)
{
}

void WglWindow::setHoverCallback(std::function<void(HoverEvent const&)> /*cb*/)
{
}

} // namespace ase

