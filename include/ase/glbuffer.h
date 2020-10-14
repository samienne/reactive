#pragma once

#include "usage.h"

#include "asevisibility.h"

#include "systemgl.h"

#include <cstring>

namespace ase
{
    class GlPlatform;
    class GlRenderContext;
    struct Dispatched;
    struct GlFunctions;

    class ASE_EXPORT GlBuffer
    {
    public:
        GlBuffer(GlRenderContext& context, GLenum bufferType);
        ~GlBuffer();

        void setData(Dispatched, GlFunctions const& gl, void const* data,
                size_t len, Usage usage);
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

