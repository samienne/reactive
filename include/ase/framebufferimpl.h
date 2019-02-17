#pragma once

#include <btl/visibility.h>

#include <memory>

namespace ase
{
    class Texture;
    class Renderbuffer;

    class BTL_VISIBLE FramebufferImpl
    {
    public:
        virtual ~FramebufferImpl() = default;

        virtual void setColorTarget(size_t index, Texture texture) = 0;
        virtual void setColorTarget(size_t index, Renderbuffer texture) = 0;
        virtual void unsetColorTarget(size_t index) = 0;

        virtual void setDepthTarget(Renderbuffer buffer) = 0;
        virtual void unsetDepthTarget() = 0;

        virtual void setStencilTarget(Renderbuffer buffer) = 0;
        virtual void unsetStencilTarget() = 0;

        virtual void clear() = 0;
    };
} // namespace ase

