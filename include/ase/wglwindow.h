#pragma once

#include "vector.h"
#include "window.h"
#include "pointerbuttonevent.h"
#include "pointermoveevent.h"
#include "pointerdragevent.h"
#include "hoverevent.h"
#include "keyevent.h"

#include "asevisibility.h"

#include <mutex>
#include <vector>

namespace ase
{
    class WglContext;
    class WglPlatform;
    class WglWindowDeferred;
    class RenderContext;
    class RenderCommand;
    class Framebuffer;
    struct Dispatched;

    class ASE_EXPORT WglWindow : public Window
    {
    public:
        typedef std::mutex Mutex;
        typedef std::unique_lock<Mutex> Lock;

        WglWindow(WglPlatform& platform, Vector2i const& size);
        WglWindow(WglWindow&&) = delete;
        WglWindow(WglWindow const&) = delete;
        ~WglWindow();

        WglWindow& operator=(WglWindow&&) = delete;
        WglWindow& operator=(WglWindow const&) = delete;

        void setCloseCallback(std::function<void()> func);
        void setResizeCallback(std::function<void()> func);
        void setRedrawCallback(std::function<void()> func);
        void setButtonCallback(
                std::function<void(PointerButtonEvent const&)> cb);
        void setPointerCallback(
                std::function<void(PointerMoveEvent const&)> cb);
        void setDragCallback(
                std::function<void(PointerDragEvent const&)> cb);
        void setKeyCallback(std::function<void(KeyEvent const&)> cb);
        void setHoverCallback(std::function<void(HoverEvent const&)> cb);

        Vector2i getSize() const;
        Framebuffer& getDefaultFramebuffer();

    private:
        void present(Dispatched);

        friend class WglContext;
        friend class WglFramebuffer;
        void makeCurrent(Lock const& lock, WglContext const& context) const;

    private:
        friend class WglWindowDeferred;
        inline WglWindowDeferred* d()
        {
            return reinterpret_cast<WglWindowDeferred*>(Window::d());
        }

        inline WglWindowDeferred const* d() const
        {
            return reinterpret_cast<WglWindowDeferred const*>(Window::d());
        }
    };
}
