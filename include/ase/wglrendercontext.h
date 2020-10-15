#pragma once

#include "glrendercontext.h"

namespace ase
{
    class WglPlatform;
    class WglDispatchedContext;

    class ASE_EXPORT WglRenderContext : public GlRenderContext
    {
    public:
        WglRenderContext(WglPlatform& platform);

        WglDispatchedContext const& getWglContext() const;

        void present(Window& window) override;

    private:
        friend class WglFramebuffer;

    private:
        WglPlatform& platform_;
    };
} // namespace ase

