#pragma once

#include "dispatcher.h"

#include <btl/visibility.h>

#include <GL/gl.h>

namespace ase
{
    class Texture;
    class Dispatcher;
    class GlTexture;
    class GlPlatform;
    class GlRenderContext;

    class BTL_VISIBLE GlFramebuffer
    {
    public:
        GlFramebuffer(GlRenderContext& context);
        GlFramebuffer(Dispatched, GlRenderContext& context);

        GlFramebuffer(GlFramebuffer const& rhs) = delete;
        GlFramebuffer(GlFramebuffer&& rhs) noexcept;

        ~GlFramebuffer();

        void destroy(Dispatched, GlRenderContext& context);

        bool operator==(GlFramebuffer const& rhs) const;
        bool operator!=(GlFramebuffer const& rhs) const;
        bool operator<(GlFramebuffer const& rhs) const;

        operator bool() const;

        void setColorTarget(Dispatched, size_t index, Texture const& texture);
        void setColorTarget(Dispatched, size_t index, GlTexture const& texture);

        void makeCurrent(Dispatched) const;

    private:
        GlRenderContext& context_;
        GLuint framebuffer_ = 0;
    };
}

