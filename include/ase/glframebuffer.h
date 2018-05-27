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
        GlFramebuffer();
        GlFramebuffer(Dispatched, GlPlatform& platform,
                GlRenderContext& context);
        GlFramebuffer(GlFramebuffer const& rhs) = delete;
        GlFramebuffer(GlFramebuffer&& rhs) = default;
        ~GlFramebuffer();

        void destroy(Dispatched, GlRenderContext& context);

        GlFramebuffer& operator=(GlFramebuffer const& rhs) = delete;
        GlFramebuffer& operator=(GlFramebuffer&& rhs) noexcept;

        bool operator==(GlFramebuffer const& rhs) const;
        bool operator!=(GlFramebuffer const& rhs) const;
        bool operator<(GlFramebuffer const& rhs) const;

        operator bool() const;

        void setColorTarget(Dispatched, GlRenderContext& context, size_t index,
                Texture const& texture);
        void setColorTarget(Dispatched, GlRenderContext& context, size_t index,
                GlTexture const& texture);

        void makeCurrent(Dispatched, GlRenderContext& context) const;

    private:
        GlPlatform* platform_;
        GLuint framebuffer_;
    };
}

