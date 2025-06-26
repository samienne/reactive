#pragma once

#include "framebufferimpl.h"

#include "asevisibility.h"

namespace ase
{
    struct Dispatched;
    struct GlFunctions;
    class GlRenderState;
    class GlDispatchedContext;

    class ASE_EXPORT GlBaseFramebuffer : public FramebufferImpl
    {
    public:
        virtual void makeCurrent(
                Dispatched,
                GlDispatchedContext& context,
                GlRenderState& renderState,
                GlFunctions const& gl) const = 0;
    };
} // namespace ase

