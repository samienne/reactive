#include "wglwindow.h"

#include "wglplatform.h"
#include "dummyframebuffer.h"

#include <windows.h>

#include <GL/gl.h>
#include <GL/wglext.h>

#include <memory>

namespace ase
{

WglWindow::WglWindow(WglPlatform& platform, Vector2i size) :
    platform_(platform),
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

    HDC dc = GetDC(hwnd_);

    PIXELFORMATDESCRIPTOR pfd = {0};

    pfd.nSize = sizeof(pfd);
    pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 32;

    int n = ChoosePixelFormat(dc, &pfd);
    SetPixelFormat(dc, n, &pfd);

    auto context = platform.createRawContext(2, 0);
    wglMakeCurrent(dc, context);
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

