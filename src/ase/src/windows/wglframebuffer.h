#pragma once

#include "glbaseframebuffer.h"

namespace ase
{
    class WglWindow;

    class WglFramebuffer : public GlBaseFramebuffer
    {
    public:
        explicit WglFramebuffer(WglWindow& window);

        WglFramebuffer(WglFramebuffer const&) = delete;
        WglFramebuffer(WglFramebuffer&&) = delete;

        WglFramebuffer& operator=(WglFramebuffer const&) = delete;
        WglFramebuffer& operator=(WglFramebuffer&&) = delete;

        // From GlBaseFramebuffer
        void makeCurrent(Dispatched, GlDispatchedContext& context,
                GlRenderState& renderState,
                GlFunctions const& gl) const override;

        // From FramebufferImpl
        void setColorTarget(size_t index, Texture texture) override;
        void setColorTarget(size_t index, Renderbuffer texture) override;
        void unsetColorTarget(size_t index) override;

        void setDepthTarget(Renderbuffer buffer) override;
        void unsetDepthTarget() override;

        void setStencilTarget(Renderbuffer buffer) override;
        void unsetStencilTarget() override;

    private:
        WglWindow& window_;
    };
} // namespace ase



