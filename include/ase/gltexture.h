#pragma once

#include "textureimpl.h"
#include "vector.h"
#include "format.h"

#include "asevisibility.h"

#include <GL/gl.h>

namespace ase
{
    class GlPlatform;
    class GlRenderContext;
    class Buffer;
    struct Dispatched;
    struct GlFunctions;

    class ASE_EXPORT GlTexture : public TextureImpl
    {
    public:
        GlTexture(GlRenderContext& context);
        GlTexture(GlTexture const& other) = delete;
        GlTexture(GlTexture&& other) = delete;
        ~GlTexture();

        GlTexture& operator=(GlTexture const& other) = delete;
        GlTexture& operator=(GlTexture&& other) = delete;

        void setData(Dispatched, GlFunctions const& gl, Vector2i const& size,
                Format format,
                Buffer const& buffer);

        GLuint getGlObject() const;

        // From TextureImpl
        virtual Vector2i getSize() const;

    private:
        void destroy();

    private:
        friend class GlRenderContext;
        friend class GlFramebuffer;
        GlRenderContext& context_;
        Vector2i size_;
        GLuint texture_;
        Format format_;
    };
}

