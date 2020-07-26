#pragma once

#include "glbaseframebuffer.h"

#include "dispatcher.h"

#include "asevisibility.h"

#include <GL/gl.h>

namespace ase
{
    class Texture;
    class Dispatcher;
    class GlTexture;
    class GlPlatform;
    class GlRenderContext;
    struct GlFunctions;

    class ASE_EXPORT GlFramebuffer : public GlBaseFramebuffer
    {
    public:
        GlFramebuffer();
        GlFramebuffer(GlRenderContext& context);
        GlFramebuffer(Dispatched, GlFunctions const& gl,
                GlRenderContext& context);

        static GlFramebuffer makeDefault(GlRenderContext& context);

        GlFramebuffer(GlFramebuffer const& rhs) = delete;
        GlFramebuffer(GlFramebuffer&& rhs) noexcept;

        ~GlFramebuffer();

        void destroy(Dispatched, GlFunctions const&);

        bool operator==(GlFramebuffer const& rhs) const;
        bool operator!=(GlFramebuffer const& rhs) const;
        bool operator<(GlFramebuffer const& rhs) const;

        operator bool() const;

        /*
        void setColorTarget(Dispatched, GlFunctions const& gl, size_t index,
                Texture const& texture);
        void setColorTarget(Dispatched, GlFunctions const& gl, size_t index,
                GlTexture const& texture);
                */

        void makeCurrent(Dispatched, GlRenderContext& context,
                GlFunctions const& gl) const override;

        // FramebufferImpl
        void setColorTarget(size_t index, Texture texture) override;
        void setColorTarget(size_t index, Renderbuffer texture) override;
        void unsetColorTarget(size_t index) override;

        void setDepthTarget(Renderbuffer buffer) override;
        void unsetDepthTarget() override;

        void setStencilTarget(Renderbuffer buffer) override;
        void unsetStencilTarget() override;

        void clear() override;

    private:
        friend class GlRenderContext;
        GlFramebuffer(GlRenderContext& context, std::nullptr_t);

    private:
        GlRenderContext& context_;
        GLuint framebuffer_ = 0;

        struct Attachment
        {
            GLenum target = 0;
            GLuint object = 0;
        };

        std::array<Attachment, 8> colorAttachments_;
        GLuint depthAttachment_ = 0;
        GLuint stencilAttachment_ = 0;
        mutable bool dirty_ = true;
    };
}

