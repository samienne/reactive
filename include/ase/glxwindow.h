#pragma once

#include "glxframebuffer.h"
#include "glxdispatchedcontext.h"
#include "glxrendercontext.h"
#include "glxcontext.h"
#include "glxplatform.h"

#include "vector.h"
#include "window.h"
#include "pointerbuttonevent.h"
#include "pointermoveevent.h"
#include "pointerdragevent.h"
#include "hoverevent.h"
#include "keyevent.h"
#include "windowimpl.h"
#include "framebuffer.h"
#include "genericwindow.h"
#include "windowimpl.h"

#include "asevisibility.h"

#include <tracy/Tracy.hpp>

#include <GL/gl.h>
#include <GL/glxext.h>
#include <GL/glx.h>

#include <X11/extensions/sync.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <X11/X.h>

#include <mutex>
#include <vector>

union _XEvent;

namespace ase
{
    class GlxContext;
    class GlxPlatform;
    class GlxWindowDeferred;
    class RenderContext;
    class Framebuffer;
    struct Dispatched;

    class ASE_EXPORT GlxWindow : public WindowImpl
    {
    public:
        typedef std::mutex Mutex;
        typedef std::unique_lock<LockableBase(Mutex)> Lock;

        GlxWindow(GlxPlatform& platform, Vector2i const& size, float scalingFactor);
        GlxWindow(GlxWindow&&) = delete;
        GlxWindow(GlxWindow const&) = delete;
        ~GlxWindow();

        GlxWindow& operator=(GlxWindow&&) = delete;
        GlxWindow& operator=(GlxWindow const&) = delete;

        std::optional<std::chrono::microseconds> frame(Frame const& frame);
        void handleEvents(std::vector<_XEvent> const& events);

        bool needsRedraw() const;

        // From WindowImpl
        void setVisible(bool value) override;
        bool isVisible() const override;

        void setTitle(std::string&& title) override;
        std::string const& getTitle() const override;

        Vector2i getSize() const override;
        float getScalingFactor() const override;
        Framebuffer& getDefaultFramebuffer() override;

        void requestFrame() override;

        void setFrameCallback(
                std::function<std::optional<std::chrono::microseconds>(Frame const&)>)
            override;
        void setCloseCallback(std::function<void()> func) override;
        void setResizeCallback(std::function<void()> func) override;
        void setButtonCallback(
                std::function<void(PointerButtonEvent const&)> cb) override;
        void setPointerCallback(
                std::function<void(PointerMoveEvent const&)> cb) override;
        void setDragCallback(
                std::function<void(PointerDragEvent const&)> cb) override;
        void setKeyCallback(std::function<void(KeyEvent const&)> cb) override;
        void setHoverCallback(std::function<void(HoverEvent const&)> cb) override;
        void setTextCallback(std::function<void(TextEvent const&)> cb) override;

        Vector2i getResolution() const;

    private:
        void destroy();

        friend class GlxRenderContext;
        void handleEvent(_XEvent const& e);
        void present(Dispatched);
        Lock lockX() const;

        friend class GlxContext;
        friend class GlxFramebuffer;
        void makeCurrent(Lock const& lock, GlxContext const& context) const;

    private:
        GlxPlatform& platform_;
        ::Window xWin_ = 0;
        ::GLXWindow glxWin_ = 0;
        XIM xim_ = nullptr;
        XIC xic_ = nullptr;
        XID syncCounter_ = 0;
        int64_t counterValue_ = 0;

        GenericWindow genericWindow_;
        Framebuffer defaultFramebuffer_;

        bool visible_ = false;

        // Text input handling
        //XComposeStatus composeStatus_;

        // Atoms
        Atom wmDelete_;
        Atom wmProtocols_;
        Atom wmSyncRequest_;

        // counters
        unsigned int frames_ = 0;
    };
}

