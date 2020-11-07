#pragma once

#include "renderbufferimpl.h"
#include "format.h"
#include "vector.h"

#include "asevisibility.h"

#include "systemgl.h"

namespace ase
{
    class GlRenderContext;

    class ASE_EXPORT GlRenderbuffer : public RenderbufferImpl
    {
    public:
        GlRenderbuffer(GlRenderContext& context, Vector2i size, Format format);

        GLuint getGlObject() const;

        // From RenderbufferImpl
        Format getFormat() const override;
        Vector2i getSize() const override;

    private:
        GlRenderContext& context_;
        Format format_ = FORMAT_UNKNOWN;
        GLuint glObject_ = 0;
        Vector2i size_;
    };
} // ase

