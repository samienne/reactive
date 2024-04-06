#pragma once

#include "glplatform.h"

#include "asevisibility.h"

#include <tracy/Tracy.hpp>

#include <GL/glx.h>
#include <X11/Xlib.h>

#include <mutex>

namespace ase
{
    class GlxWindow;
    class GlxContext;
    class GlxPlatformDeferred;

    class ASE_EXPORT DefaultGlPlatform : public GlPlatform
    {
    public:
        virtual std::vector<DefaultGlWindow> getWindows() const = 0;
        // From Platform
        void run() override;
    private:
    };

    class ASE_EXPORT GlxPlatform : public DefaultGlPlatform
    {
    public:
        typedef std::mutex Mutex;
        typedef std::unique_lock<LockableBase(Mutex)> Lock;

        GlxPlatform();
        GlxPlatform(GlxPlatform const& other) = delete;
        ~GlxPlatform();

        GlxPlatform const& operator=(GlxPlatform const& other) = delete;

        Display* getDisplay();

        Lock lockX();

        std::vector<XEvent> getEvents(Lock const&);

        float getScalingFactor() const;

        // From Platform
        Window makeWindow(Vector2i size) override;
        void handleEvents() override;
        RenderContext makeRenderContext() override;

    private:
        friend class GlxRenderContext;
        friend class GlxContext;
        friend class GlxPlatformDeferred;

        GLXContext createGlxContext(Lock const& lock);
        void destroyGlxContext(Lock const& lock, GLXContext context);
        void makeGlxContextCurrent(Lock const& lock, GLXContext context,
                GLXDrawable drawable);
        void swapGlxBuffers(Lock const& lock, GLXDrawable drawable);

        friend class GlxWindow;
        friend class GlxWindowDeferred;
        void registerWindow(Lock const&, GlxWindow& window);
        void unregisterWindow(Lock const&, GlxWindow& window);
        GLXFBConfig getGlxFbConfig() const;

    private:
        inline GlxPlatformDeferred* d() { return deferred_; }
        inline GlxPlatformDeferred const* d() const { return deferred_; }
        GlxPlatformDeferred* deferred_;
    };
}

