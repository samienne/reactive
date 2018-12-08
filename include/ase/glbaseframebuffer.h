#pragma once

#include "framebufferimpl.h"

namespace ase
{
    class Dispatched;
    class GlRenderContext;
    struct GlFunctions;

    class GlBaseFramebuffer : public FramebufferImpl
    {
    public:
        virtual void makeCurrent(Dispatched, GlRenderContext& context,
                GlFunctions const& gl) const = 0;
    };
} // namespace ase

