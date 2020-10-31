#include "wglwindow.h"

#include "wglplatform.h"
#include "wglframebuffer.h"

#include <windows.h>

#include <utf8/utf8.h>

#include <GL/gl.h>
#include <GL/wglext.h>

#include <windowsx.h>
#include <shtypes.h>
#include <shellscalingapi.h>
#include <winuser.h>

#include <memory>
#include <iostream>
#include <codecvt>

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

    std::string convertFromUtf16(wchar_t c)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;

        wchar_t buf[2];
        buf[0] = c;
        buf[1] = 0;

        return convert.to_bytes(buf);
    }

    uint32_t getKeyModifiers()
    {
        uint32_t result = 0;

        bool shift = (GetKeyState(VK_SHIFT) & (1 << 31)) != 0;
        bool alt = (GetKeyState(VK_MENU) & (1 << 31)) != 0;
        bool winL = (GetKeyState(VK_LWIN) & (1 << 31)) != 0;
        bool winR = (GetKeyState(VK_RWIN) & (1 << 31)) != 0;
        bool control = (GetKeyState(VK_CONTROL) & (1 << 31)) != 0;

        return (shift ? (uint32_t)KeyModifier::Shift : 0)
            | (alt ? (uint32_t)KeyModifier::Alt : 0)
            | ((winL || winR) ? (uint32_t)KeyModifier::Meta : 0)
            | (control ? (uint32_t)KeyModifier::Control : 0)
            ;
    }

    void printModifiers(uint32_t modifiers)
    {
        std::cout << "modifiers: "
            << "s: " << ((modifiers & (uint32_t)KeyModifier::Shift) != 0) << " "
            << "a: " << ((modifiers & (uint32_t)KeyModifier::Alt) != 0) << " "
            << "m: " << ((modifiers & (uint32_t)KeyModifier::Meta) != 0) << " "
            << "c: " << ((modifiers & (uint32_t)KeyModifier::Control) != 0)
            << std::endl;
    }

    KeyCode translateKey(uint32_t c)
    {
        switch (c)
        {
            //case VK_LBUTTON:
            //case VK_RBUTTON:
            case VK_CANCEL:
                return KeyCode::cancel;
            //case VK_MBUTTON:
            //case VK_XBUTTON1:
            //case VK_XBUTTON2:
            case VK_BACK:
                return KeyCode::backSpace;
            case VK_TAB:
                return KeyCode::tab;
            case VK_CLEAR:
                return KeyCode::clear;
            case VK_RETURN:
                return KeyCode::returnKey;
            case VK_SHIFT:
                return KeyCode::shiftL;
            case VK_CONTROL:
                return KeyCode::controlL;
            case VK_MENU:
                return KeyCode::menu;
            case VK_PAUSE:
                return KeyCode::pause;
            case VK_CAPITAL:
                return KeyCode::capsLock;
            //case VK_KANA:
            //case VK_HANGUEL:
            //case VK_HANGUL:
            //case VK_IME_ON:
            //case VK_JUNJA:
            //case VK_FINAL:
            //case VK_HANJA:
            //case VK_KANJI:
            //case VK_IME_OFF:
            case VK_ESCAPE:
                return KeyCode::escape;
            //case VK_CONVERT:

            //case VK_NONCONVERT:
            //case VK_ACCEPT:
            //case VK_MODECHANGE:
            case VK_SPACE:
                return KeyCode::space;
            case VK_PRIOR:
                return KeyCode::prior;
            case VK_NEXT:
                return KeyCode::next;
            case VK_END:
                return KeyCode::end;
            case VK_HOME:
                return KeyCode::home;
            case VK_LEFT:
                return KeyCode::left;
            case VK_UP:
                return KeyCode::up;
            case VK_RIGHT:
                return KeyCode::right;
            case VK_DOWN:
                return KeyCode::down;
            case VK_SELECT:
                return KeyCode::select;
            case VK_PRINT:
                return KeyCode::print;
            case VK_EXECUTE:
                return KeyCode::execute;
            //case VK_SNAPSHOT:
            case VK_INSERT:
                return KeyCode::insert;
            case VK_DELETE:
                return KeyCode::kpDelete;
            case VK_HELP:
                return KeyCode::help;
            case 0x30:
                return KeyCode::key0;
            case 0x31:
                return KeyCode::key1;
            case 0x32:
                return KeyCode::key2;
            case 0x33:
                return KeyCode::key3;
            case 0x34:
                return KeyCode::key4;
            case 0x35:
                return KeyCode::key5;
            case 0x36:
                return KeyCode::key6;
            case 0x37:
                return KeyCode::key7;
            case 0x38:
                return KeyCode::key8;
            case 0x39:
                return KeyCode::key9;
            case 0x41:
                return KeyCode::A;
            case 0x42:
                return KeyCode::B;
            case 0x43:
                return KeyCode::C;
            case 0x44:
                return KeyCode::D;
            case 0x45:
                return KeyCode::E;
            case 0x46:
                return KeyCode::F;
            case 0x47:
                return KeyCode::G;
            case 0x48:
                return KeyCode::H;
            case 0x49:
                return KeyCode::I;
            case 0x4A:
                return KeyCode::J;
            case 0x4B:
                return KeyCode::K;
            case 0x4C:
                return KeyCode::L;
            case 0x4D:
                return KeyCode::M;
            case 0x4E:
                return KeyCode::N;
            case 0x4F:
                return KeyCode::O;
            case 0x50:
                return KeyCode::P;
            case 0x51:
                return KeyCode::Q;
            case 0x52:
                return KeyCode::R;
            case 0x53:
                return KeyCode::S;
            case 0x54:
                return KeyCode::T;
            case 0x55:
                return KeyCode::U;
            case 0x56:
                return KeyCode::V;
            case 0x57:
                return KeyCode::W;
            case 0x58:
                return KeyCode::X;
            case 0x59:
                return KeyCode::Y;
            case 0x5A:
                return KeyCode::Z;
            case VK_LWIN:
                return KeyCode::superL;
            case VK_RWIN:
                return KeyCode::superR;
            //case VK_APPS:
            //case VK_SLEEP:
            case VK_NUMPAD0:
                return KeyCode::kp0;
            case VK_NUMPAD1:
                return KeyCode::kp1;
            case VK_NUMPAD2:
                return KeyCode::kp2;
            case VK_NUMPAD3:
                return KeyCode::kp3;
            case VK_NUMPAD4:
                return KeyCode::kp4;
            case VK_NUMPAD5:
                return KeyCode::kp5;
            case VK_NUMPAD6:
                return KeyCode::kp6;
            case VK_NUMPAD7:
                return KeyCode::kp7;
            case VK_NUMPAD8:
                return KeyCode::kp8;
            case VK_NUMPAD9:
                return KeyCode::kp9;
            case VK_MULTIPLY:
                return KeyCode::kpMultiply;
            case VK_ADD:
                return KeyCode::kpAdd;
            case VK_SEPARATOR:
                return KeyCode::kpSeparator;
            case VK_SUBTRACT:
                return KeyCode::kpSubtract;
            case VK_DECIMAL:
                return KeyCode::kpDecimal;
            case VK_DIVIDE:
                return KeyCode::kpDivide;
            case VK_F1:
                return KeyCode::f1;
            case VK_F2:
                return KeyCode::f2;
            case VK_F3:
                return KeyCode::f3;
            case VK_F4:
                return KeyCode::f4;
            case VK_F5:
                return KeyCode::f5;
            case VK_F6:
                return KeyCode::f6;
            case VK_F7:
                return KeyCode::f7;
            case VK_F8:
                return KeyCode::f8;
            case VK_F9:
                return KeyCode::f9;
            case VK_F10:
                return KeyCode::f10;
            case VK_F11:
                return KeyCode::f11;
            case VK_F12:
                return KeyCode::f12;
            case VK_F13:
                return KeyCode::f13;
            case VK_F14:
                return KeyCode::f14;
            case VK_F15:
                return KeyCode::f15;
            case VK_F16:
                return KeyCode::f16;
            case VK_F17:
                return KeyCode::f17;
            case VK_F18:
                return KeyCode::f18;
            case VK_F19:
                return KeyCode::f19;
            case VK_F20:
                return KeyCode::f20;
            case VK_F21:
                return KeyCode::f21;
            case VK_F22:
                return KeyCode::f22;
            case VK_F23:
                return KeyCode::f23;
            case VK_F24:
                return KeyCode::f24;
            case VK_NUMLOCK:
                return KeyCode::numLock;
            case VK_SCROLL:
                return KeyCode::scrollLock;
            case VK_LSHIFT:
                return KeyCode::shiftL;
            case VK_RSHIFT:
                return KeyCode::shiftR;
            case VK_LCONTROL:
                return KeyCode::controlL;
            case VK_RCONTROL:
                return KeyCode::controlR;
            case VK_LMENU:
                return KeyCode::menu;
            case VK_RMENU:
                return KeyCode::menu;
            //case VK_BROWSER_BACK:
            //case VK_BROWSER_FORWARD:
            //case VK_BROWSER_REFRESH:
            //case VK_BROWSER_STOP:
            //case VK_BROWSER_SEARCH:
            //case VK_BROWSER_FAVORITES:
            //case VK_BROWSER_HOME:
            //case VK_VOLUME_MUTE:
            //case VK_VOLUME_DOWN:
            //case VK_VOLUME_UP:
            //case VK_MEDIA_NEXT_TRACK:
            //case VK_MEDIA_PREV_TRACK:
            //case VK_MEDIA_STOP:
            //case VK_MEDIA_PLAY_PAUSE:
            //case VK_LAUNCH_MAIL:
            //case VK_LAUNCH_MEDIA_SELECT:
            //case VK_LAUNCH_APP1:
            //case VK_LAUNCH_APP2:
            //case VK_OEM_1:
            case VK_OEM_PLUS:
                return KeyCode::plus;
            case VK_OEM_COMMA:
                return KeyCode::comma;
            case VK_OEM_MINUS:
                return KeyCode::minus;
            case VK_OEM_PERIOD:
                return KeyCode::period;
            //case VK_OEM_2:
            //case VK_OEM_3:
            //case VK_OEM_4:
            //case VK_OEM_5:
            //case VK_OEM_6:
            //case VK_OEM_7:
            //case VK_OEM_8:
            //case VK_OEM_102:
            //case VK_PROCESSKEY:
            //case VK_PACKET:
            //case VK_ATTN:
            //case VK_CRSEL:
            //case VK_EXSEL:
            //case VK_EREOF:
            case VK_PLAY:
                return KeyCode::Play;
            //case VK_ZOOM:
            //case VK_NONAME:
            //case VK_PA1:
            //case VK_OEM_CLEAR:

            default:
        return KeyCode::unknown;
        }
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

void WglWindow::setTextCallback(std::function<void(TextEvent const&)> cb)
{
    genericWindow_.setTextCallback(std::move(cb));
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

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            //printModifiers(getKeyModifiers());

            bool down = uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN;

            genericWindow_.injectKeyEvent(
                    down ? KeyState::down : KeyState::up,
                    translateKey(wParam),
                    getKeyModifiers(),
                    ""
                    );

            break;
        }

        case WM_CHAR:
        {
            if (wParam != VK_BACK) // Skip backspace
                genericWindow_.injectTextEvent(convertFromUtf16(wParam));
            break;
        }

        case WM_UNICHAR:
        {
            std::cout << utf8::encode(wParam) << std::endl;
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

