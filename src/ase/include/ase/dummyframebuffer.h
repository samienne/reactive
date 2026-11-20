#pragma once

#include "framebufferimpl.h"

namespace ase
{
    class ASE_EXPORT DummyFramebuffer : public FramebufferImpl
    {
    public:
        void setColorTarget(size_t index, Texture texture) override;
        void unsetColorTarget(size_t index) override;

        void setDepthTarget(Renderbuffer buffer) override;
        void unsetDepthTarget() override;
    };
} // namespace ase

