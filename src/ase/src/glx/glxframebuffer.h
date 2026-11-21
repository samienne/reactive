#pragma once

#include "glbaseframebuffer.h"
#include "vector.h"

namespace ase
{
    class GlDispatchedContext;
    class GlxWindow;

    class GlxFramebuffer : public GlBaseFramebuffer
    {
    public:
        explicit GlxFramebuffer(GlxWindow& window);

        GlxFramebuffer(GlxFramebuffer const&) = delete;
        GlxFramebuffer(GlxFramebuffer&&) = delete;

        GlxFramebuffer& operator=(GlxFramebuffer const&) = delete;
        GlxFramebuffer& operator=(GlxFramebuffer&&) = delete;

        /**
         * Sets the window size in pixels; render thread only.
         */
        void setPixelSize(Vector2i size);

        // From GlBaseFramebuffer
        void makeCurrent(
                Dispatched,
                GlDispatchedContext&,
                GlRenderState& renderState,
                GlFunctions const& gl
                ) const override;

        // From FramebufferImpl
        void setColorTarget(size_t index, Texture texture) override;
        void setColorTarget(size_t index, Renderbuffer texture) override;
        void unsetColorTarget(size_t index) override;

        void setDepthTarget(Renderbuffer buffer) override;
        void unsetDepthTarget() override;

        void setStencilTarget(Renderbuffer buffer) override;
        void unsetStencilTarget() override;

    private:
        GlxWindow& window_;
        Vector2i pixelSize_ = Vector2i::Zero();
    };
} // namespace ase


