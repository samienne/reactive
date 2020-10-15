#include "wglwindow.h"

#include "wglplatform.h"
#include "wglframebuffer.h"

#include <windows.h>

#include <GL/gl.h>
#include <GL/wglext.h>

#include <memory>

namespace ase
{

WglWindow::WglWindow(WglPlatform& platform, Vector2i size,
        float scalingFactor) :
    platform_(platform),
    genericWindow_(size, scalingFactor),
    defaultFramebuffer_(std::make_shared<WglFramebuffer>(platform, *this))
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

    HDC dc = GetDC(hwnd_);

    auto pfd = platform_.getPixelFormatDescriptor();

    int n = ChoosePixelFormat(dc, &pfd);
    SetPixelFormat(dc, n, &pfd);

    auto context = platform.createRawContext(2, 0);
    wglMakeCurrent(dc, context);
}

HDC WglWindow::getDc() const
{
    return GetDC(hwnd_);
}

void WglWindow::present()
{
    glClearColor(0.0, 0.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    SwapBuffers(GetDC(hwnd_));
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

void WglWindow::setCloseCallback(std::function<void()> func)
{
    genericWindow_.setCloseCallback(std::move(func));
}

void WglWindow::setResizeCallback(std::function<void()> func)
{
    genericWindow_.setResizeCallback(std::move(func));
}

void WglWindow::setRedrawCallback(std::function<void()> func)
{
    genericWindow_.setRedrawCallback(std::move(func));
}

void WglWindow::setButtonCallback(
        std::function<void(PointerButtonEvent const& e)> cb)
{
    genericWindow_.setButtonCallback(std::move(cb));
}

void WglWindow::setPointerCallback(
        std::function<void(PointerMoveEvent const&)> cb)
{
    genericWindow_.setPointerCallback(std::move(cb));
}

void WglWindow::setDragCallback(
        std::function<void(PointerDragEvent const&)> cb)
{
    genericWindow_.setDragCallback(std::move(cb));
}

void WglWindow::setKeyCallback(std::function<void(KeyEvent const&)> cb)
{
    genericWindow_.setKeyCallback(std::move(cb));
}

void WglWindow::setHoverCallback(std::function<void(HoverEvent const&)> cb)
{
    genericWindow_.setHoverCallback(std::move(cb));
}

} // namespace ase

