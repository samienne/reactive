#pragma once

#include "vector.h"
#include "avgvisibility.h"

#include <ase/format.h>
#include <ase/texture.h>
#include <ase/renderbuffer.h>
#include <ase/framebuffer.h>
#include <ase/rendercontext.h>

namespace avg
{
    class AVG_EXPORT TargetImage
    {
    public:
        TargetImage(ase::RenderContext& context, Vector2i size,
                ase::Format format);
        Vector2i getSize() const;
        ase::Texture& getTexture();
        ase::Framebuffer& getFramebuffer();

    private:
        ase::Texture texture_;
        ase::Renderbuffer depthbuffer_;
        ase::Framebuffer framebuffer_;
    };

} // namespace avg

