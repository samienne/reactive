#pragma once

#include "glbaseframebuffer.h"

namespace ase
{
    class GlxWindow;
    class GlxPlatform;

    class GlxFramebuffer : public GlBaseFramebuffer
    {
    public:
        GlxFramebuffer(GlxPlatform& platform, GlxWindow& window);

        GlxFramebuffer(GlxFramebuffer const&) = delete;
        GlxFramebuffer(GlxFramebuffer&&) = delete;

        GlxFramebuffer& operator=(GlxFramebuffer const&) = delete;
        GlxFramebuffer& operator=(GlxFramebuffer&&) = delete;

        // From GlBaseFramebuffer
        void makeCurrent(Dispatched, GlRenderContext& context,
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
        GlxPlatform& platform_;
        GlxWindow& window_;
    };
} // namespace ase


