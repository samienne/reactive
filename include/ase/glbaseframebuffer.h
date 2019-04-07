#pragma once

#include "framebufferimpl.h"

namespace ase
{
    struct Dispatched;
    struct GlFunctions;
    class GlRenderContext;

    class GlBaseFramebuffer : public FramebufferImpl
    {
    public:
        virtual void makeCurrent(Dispatched, GlRenderContext& context,
                GlFunctions const& gl) const = 0;
    };
} // namespace ase

