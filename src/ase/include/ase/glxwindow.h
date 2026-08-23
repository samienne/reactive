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
#include "windowbase.h"
#include "framebuffer.h"

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

#include <cstdint>
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

    class ASE_EXPORT GlxWindow : public WindowBase
    {
    public:
        typedef std::mutex Mutex;
        typedef std::unique_lock<LockableBase(Mutex)> Lock;

        GlxWindow(GlxPlatform& platform, RenderContext& context,
                Vector2i const& size, float scalingFactor);
        GlxWindow(GlxWindow&&) = delete;
        GlxWindow(GlxWindow const&) = delete;
        ~GlxWindow();

        GlxWindow& operator=(GlxWindow&&) = delete;
        GlxWindow& operator=(GlxWindow const&) = delete;

        void handleEvents(std::vector<_XEvent> const& events);

        PresentStatus present() override;

        // From WindowImpl
        void setVisible(bool value) override;
        bool isVisible() const override;

        void setTitle(std::string&& title) override;
        std::string const& getTitle() const override;

        Framebuffer& getDefaultFramebuffer() override;

        void requestFrame() override;

        Vector2i getResolution() const;

    private:
        std::optional<std::chrono::microseconds> frame(
                Frame const& frame) override;

        void destroy();

        void handleEvent(_XEvent const& e);
        Lock lockX() const;

        friend class GlxDispatchedContext;
        ::GLXWindow getGlxWindowId() const;

    private:
        GlxPlatform& platform_;
        ::Window xWin_ = 0;
        ::GLXWindow glxWin_ = 0;
        XIM xim_ = nullptr;
        XIC xic_ = nullptr;
        XID syncCounter_ = 0;
        int64_t counterValue_ = 0;

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

