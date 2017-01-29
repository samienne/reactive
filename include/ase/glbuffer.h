#pragma once

#include "usage.h"

#include <GL/gl.h>

namespace ase
{
    class GlPlatform;
    class GlRenderContext;
    class RenderContext;
    struct Dispatched;

    class GlBuffer
    {
    public:
        GlBuffer(RenderContext& context, GLenum bufferType);
        ~GlBuffer();

        void setData(Dispatched, GlRenderContext& context, void const* data,
                size_t len, Usage usage);
        GLuint getBuffer() const;

    private:
        void destroy();

    private:
        friend class GlRenderContext;

        GlPlatform* platform_;
        GLenum bufferType_;
        GLuint buffer_;
    };
}

