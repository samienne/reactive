#pragma once

#include "vector.h"
#include "window.h"
#include "pointerbuttonevent.h"
#include "pointermoveevent.h"
#include "pointerdragevent.h"
#include "hoverevent.h"
#include "keyevent.h"

#include <btl/visibility.h>

#include <mutex>
#include <vector>

union _XEvent;

namespace ase
{
    class GlxContext;
    class GlxPlatform;
    class GlxWindowDeferred;
    class RenderContext;
    class RenderCommand;
    struct Dispatched;

    class BTL_VISIBLE GlxWindow : public Window
    {
    public:
        typedef std::mutex Mutex;
        typedef std::unique_lock<Mutex> Lock;

        GlxWindow(GlxPlatform& platform, Vector2i const& size);
        GlxWindow(GlxWindow&&) = default;
        GlxWindow(GlxWindow const&) = delete;
        ~GlxWindow();

        GlxWindow& operator=(GlxWindow&&) = default;
        GlxWindow& operator=(GlxWindow const&) = delete;

        void handleEvents(std::vector<_XEvent> const& events);

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

    private:
        friend class GlxRenderContext;
        void handleEvent(_XEvent const& e);
        void present(Dispatched);
        Lock lockX() const;

        friend class GlxContext;
        void makeCurrent(Lock const& lock, GlxContext const& context) const;

    private:
        friend class GlxWindowDeferred;
        inline GlxWindowDeferred* d()
        {
            return reinterpret_cast<GlxWindowDeferred*>(Window::d());
        }

        inline GlxWindowDeferred const* d() const
        {
            return reinterpret_cast<GlxWindowDeferred const*>(Window::d());
        }
    };
}

