#pragma once

#include "glrendercontext.h"
#include "glxcontext.h"
#include "vector.h"

#include <btl/visibility.h>

namespace ase
{
    class GlxPlatform;
    class GlxDispatchedContext;

    class BTL_VISIBLE GlxRenderContext : public GlRenderContext
    {
    public:
        typedef std::mutex Mutex;
        typedef std::unique_lock<Mutex> Lock;

        GlxRenderContext(GlxPlatform& platform);
        ~GlxRenderContext();

        // From RenderContextImpl
        void present(Window& window) override;

    private:
        friend class GlxPlatform;

        GlxRenderContext(
                GlxPlatform& platform,
                std::shared_ptr<GlxDispatchedContext>&& fgContext,
                std::shared_ptr<GlxDispatchedContext>&& bgContext
                );

    private:
        friend class GlxRenderTarget;
        friend class GlxWindowDeferred;

        GlxDispatchedContext const& getGlxContext() const;

    private:
        GlxPlatform& platform_;
    };
}

