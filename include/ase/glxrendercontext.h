#pragma once

#include "glrendercontext.h"
#include "glxcontext.h"
#include "vector.h"

#include <btl/visibility.h>

namespace ase
{
    class GlxPlatform;

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
        GlxRenderContext(GlxPlatform& platform, GlxContext&& context,
                GlxContext&& contextBg);

    private:
        friend class GlxRenderTarget;
        friend class GlxWindowDeferred;
        GlxContext const& getContext() const;
        GlxContext& getContext();

    private:
        GlxPlatform& platform_;
        GlxContext context_;
        GlxContext contextBg_;
    };
}

