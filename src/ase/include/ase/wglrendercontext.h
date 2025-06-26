#pragma once

#include "glrendercontext.h"

#include "systemgl.h"

namespace ase
{
    class WglPlatform;
    class WglDispatchedContext;

    class ASE_EXPORT WglRenderContext : public GlRenderContext
    {
    public:
        WglRenderContext(WglPlatform& platform, HGLRC fgContext,
                HGLRC bgContext);

        WglDispatchedContext const& getWglContext() const;

        void present(Dispatched d, Window& window) override;

    private:
        friend class WglFramebuffer;

    private:
        WglPlatform& platform_;
    };
} // namespace ase

