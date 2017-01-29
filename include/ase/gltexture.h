#pragma once

#include "textureimpl.h"
#include "vector.h"
#include "format.h"

#include <GL/gl.h>

namespace ase
{
    class GlPlatform;
    class RenderContext;
    class Buffer;
    struct Dispatched;

    class GlTexture : public TextureImpl
    {
    public:
        GlTexture(RenderContext& context);
        GlTexture(GlTexture const& other) = delete;
        GlTexture(GlTexture&& other) = delete;
        ~GlTexture();

        GlTexture& operator=(GlTexture const& other) = delete;
        GlTexture& operator=(GlTexture&& other) = delete;

        void setData(Dispatched, Vector2i const& size, Format format,
                Buffer const& buffer);

        // From TextureImpl
        virtual Vector2i getSize() const;

    private:
        void destroy();

    private:
        friend class GlRenderContext;
        friend class GlFramebuffer;
        GlPlatform* platform_;
        Vector2i size_;
        GLuint texture_;
        Format format_;
    };
}

