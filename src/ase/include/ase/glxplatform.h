#pragma once

#include "glplatform.h"

#include "asevisibility.h"

#include <tracy/Tracy.hpp>

#include <GL/glx.h>
#include <X11/Xlib.h>

#include <string>
#include <mutex>

namespace ase
{
    class GlxWindow;
    class GlxContext;
    class GlxPlatformDeferred;

    class ASE_EXPORT GlxPlatform : public GlPlatform
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
        void run(RenderContext& renderContext,
                std::function<bool(Frame const&)> frameCallback) override;
        void requestFrame() override;

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
        GLXFBConfig getGlxFbConfig() const;

    private:
        inline GlxPlatformDeferred* d() { return deferred_; }
        inline GlxPlatformDeferred const* d() const { return deferred_; }
        GlxPlatformDeferred* deferred_;
    };
}

