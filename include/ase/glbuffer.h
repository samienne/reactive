#pragma once

#include "usage.h"

#include <btl/visibility.h>

#include <GL/gl.h>

namespace ase
{
    class GlPlatform;
    class GlRenderContext;
    struct Dispatched;

    class BTL_VISIBLE GlBuffer
    {
    public:
        GlBuffer(GlRenderContext& context, GLenum bufferType);
        ~GlBuffer();

        void setData(Dispatched, void const* data, size_t len, Usage usage);
        GLuint getBuffer() const;

    private:
        void destroy();

    private:
        friend class GlRenderContext;

        GlRenderContext& context_;
        GLenum bufferType_;
        GLuint buffer_;
    };
}

