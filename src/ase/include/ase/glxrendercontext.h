#pragma once

#include "glrendercontext.h"
#include "glxcontext.h"
#include "vector.h"

#include "asevisibility.h"

namespace ase
{
    class GlxPlatform;
    class GlxDispatchedContext;

    class ASE_EXPORT GlxRenderContext : public GlRenderContext
    {
    public:
        typedef std::mutex Mutex;
        typedef std::unique_lock<Mutex> Lock;

        GlxRenderContext(GlxPlatform& platform);
        ~GlxRenderContext();

        // From GlRenderContext
        void present(Dispatched d, Window& window) override;

    private:
        friend class GlxPlatform;

        GlxRenderContext(
                GlxPlatform& platform,
                std::shared_ptr<GlxDispatchedContext>&& fgContext,
                std::shared_ptr<GlxDispatchedContext>&& bgContext
                );

    private:
        friend class GlxWindowDeferred;
        friend class GlxFramebuffer;

        GlxDispatchedContext const& getGlxContext() const;

    private:
        GlxPlatform& platform_;
    };
}

