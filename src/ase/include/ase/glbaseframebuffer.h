#pragma once

#include "framebufferimpl.h"
#include "vector.h"

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

        /**
         * @brief The size of the colour target in pixels.
         */
        virtual Vector2i getSize() const = 0;
    };
} // namespace ase

