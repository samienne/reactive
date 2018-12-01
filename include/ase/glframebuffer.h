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
    struct GlFunctions;

    class BTL_VISIBLE GlFramebuffer
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

        void setColorTarget(Dispatched, GlFunctions const& gl, size_t index,
                Texture const& texture);
        void setColorTarget(Dispatched, GlFunctions const& gl, size_t index,
                GlTexture const& texture);

        void makeCurrent(Dispatched, GlFunctions const& gl) const;

    private:
        friend class GlRenderContext;
        GlFramebuffer(GlRenderContext& context, std::nullptr_t);

    private:
        GlRenderContext& context_;
        GLuint framebuffer_ = 0;
    };
}

