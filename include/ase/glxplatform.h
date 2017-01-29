#pragma once

#include "glplatform.h"

#include <GL/glx.h>
#include <X11/Xlib.h>

#include <string>
#include <mutex>

namespace ase
{
    class GlxContext;
    class GlxPlatformDeferred;

    class GlxPlatform : public GlPlatform
    {
    public:
        typedef std::mutex Mutex;
        typedef std::unique_lock<Mutex> Lock;

        GlxPlatform();
        GlxPlatform(GlxPlatform const& other) = delete;
        ~GlxPlatform();

        GlxPlatform const& operator=(GlxPlatform const& other) = delete;

        Display* getDisplay();

        Lock lockX();

        std::vector<XEvent> getEvents();

        // From Platform
        RenderContext& getDefaultContext() override;
        RenderContext const& getDefaultContext() const override;

    private:
        // From Platform
        std::shared_ptr<RenderContextImpl> makeRenderContextImpl() override;

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

