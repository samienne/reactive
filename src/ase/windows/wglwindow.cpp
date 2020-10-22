#include "wglwindow.h"

#include "wglplatform.h"
#include "wglframebuffer.h"

#include <windows.h>

#include <GL/gl.h>
#include <GL/wglext.h>

#include <windowsx.h>
#include <shtypes.h>
#include <shellscalingapi.h>
#include <winuser.h>

#include <memory>
#include <iostream>

namespace ase
{

namespace
{
    float getWindowScalingFactor(HWND hwnd)
    {
        int dpi = GetDpiForWindow(hwnd);
        float scale = static_cast<float>(dpi) / 96.0f;
        return scale;
    }
} // anonymous namespace

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

    scalingFactor = getWindowScalingFactor(hwnd_);
    int dpi = GetDpiForWindow(hwnd_);

    RECT rect;
    GetWindowRect(hwnd_, &rect);

    rect.right = rect.left + (int)((float)size[0] * scalingFactor);
    rect.bottom = rect.top + (int)((float)size[1] * scalingFactor);

    AdjustWindowRectExForDpi(&rect, GetWindowLong(hwnd_, GWL_STYLE),
            false, WS_EX_APPWINDOW, dpi);

    MoveWindow(hwnd_, rect.left, rect.top,
            (int)((float)size[0] * scalingFactor),
            (int)((float)size[1] * scalingFactor),
            true
            );

    genericWindow_.setScalingFactor(scalingFactor);
}

HWND WglWindow::getHwnd() const
{
    return hwnd_;
}

HDC WglWindow::getDc() const
{
    return GetDC(hwnd_);
}

void WglWindow::present()
{
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
    return genericWindow_.getSize();
}

float WglWindow::getScalingFactor() const
{
    return genericWindow_.getScalingFactor();
}

Framebuffer& WglWindow::getDefaultFramebuffer()
{
    return defaultFramebuffer_;
}

void WglWindow::clear()
{
    defaultFramebuffer_.clear();
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

LRESULT WglWindow::handleWindowsEvent(HWND hwnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_PAINT:
            break;

        case WM_DESTROY:
            std::cout << "destroy: " << hwnd << std::endl;
            //PostQuitMessage(0);
            break;

        case WM_CLOSE:
            std::cout << "close" << std::endl;
            genericWindow_.notifyClose();
            return 0;
            break;

        case WM_QUIT:
            std::cout << "quit" << std::endl;
            return 0;
            break;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        {
            if (uMsg == WM_LBUTTONDOWN)
                capturePointer();
            else
                releasePointer();

            float scale = genericWindow_.getScalingFactor();
            float x = (float)GET_X_LPARAM(lParam) / scale;
            float y = genericWindow_.getHeight() - (float)GET_Y_LPARAM(lParam)
                / scale;

            genericWindow_.injectPointerButtonEvent(0, 1, Vector2f(x, y),
                    uMsg == WM_LBUTTONDOWN ? ButtonState::down : ButtonState::up);

            break;
        }
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        {
            if (uMsg == WM_RBUTTONDOWN)
                capturePointer();
            else
                releasePointer();

            float scale = genericWindow_.getScalingFactor();
            float x = (float)GET_X_LPARAM(lParam) / scale;
            float y = genericWindow_.getHeight() - (float)GET_Y_LPARAM(lParam)
                / scale;

            genericWindow_.injectPointerButtonEvent(0, 2, Vector2f(x, y),
                    uMsg == WM_LBUTTONDOWN ? ButtonState::down : ButtonState::up);

            break;
        }

        case WM_MOUSEMOVE:
        {
            float scale = genericWindow_.getScalingFactor();
            float x = (float)GET_X_LPARAM(lParam) / scale;
            float y = genericWindow_.getHeight() - (float)GET_Y_LPARAM(lParam)
                / scale;

            genericWindow_.injectPointerMoveEvent(0, Vector2f(x, y));
            break;
        }

        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            break;
        }

        case WM_WINDOWPOSCHANGED:
            genericWindow_.setScalingFactor(getWindowScalingFactor(hwnd_));
            break;

        case WM_DPICHANGED:
        {
            RECT* r = reinterpret_cast<RECT*>(lParam);
            float scale = getWindowScalingFactor(hwnd_);

            genericWindow_.setScalingFactor(scale);
            genericWindow_.resize(Vector2i(
                        r->right - r->left / scale,
                        r->bottom - r->top / scale
                    ));

            SetWindowPos(hwnd_, NULL, r->left, r->top, r->right - r->left,
                    r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);

            break;
        }

        case WM_SIZE:
            genericWindow_.resize(Vector2i(
                        LOWORD(lParam) / genericWindow_.getScalingFactor(),
                        HIWORD(lParam) / genericWindow_.getScalingFactor()
                        ));
            break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void WglWindow::capturePointer()
{
    ++captureCount_;
    if (captureCount_ == 1)
        SetCapture(hwnd_);
}

void WglWindow::releasePointer()
{
    --captureCount_;
    if (captureCount_ == 0)
        ReleaseCapture();
}

} // namespace ase

