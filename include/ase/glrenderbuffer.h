#pragma once

#include "renderbufferimpl.h"
#include "format.h"

#include <btl/visibility.h>

#include <GL/gl.h>

namespace ase
{
    class GlRenderContext;

    class BTL_VISIBLE GlRenderbuffer : public RenderbufferImpl
    {
    public:
        GlRenderbuffer(GlRenderContext& context, int width, int height,
                Format format);

        GLuint getGlObject() const;
        bool isDefaultTarget() const;

        // From RenderbufferImpl
        Format getFormat() override;
        int getWidth() override;
        int getHeight() override;

    private:
        GlRenderContext& context_;
        Format format_ = FORMAT_UNKNOWN;
        GLuint glObject_ = 0;
        int width_ = 0;
        int height_ = 0;
    };
} // ase
