#pragma once

#include "framebufferimpl.h"

#include "asevisibility.h"

namespace ase
{
    struct Dispatched;
    struct GlFunctions;
    class GlRenderContext;

    class ASE_EXPORT GlBaseFramebuffer : public FramebufferImpl
    {
    public:
        virtual void makeCurrent(Dispatched, GlRenderContext& context,
                GlFunctions const& gl) const = 0;
    };
} // namespace ase

