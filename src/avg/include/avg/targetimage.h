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
    /**
     * @brief An offscreen render target: a colour texture of the given
     * format over a depth-stencil buffer, in a framebuffer draws can name.
     *
     * A target cannot be resized; make a new one when the size changes.
     */
    class AVG_EXPORT TargetImage
    {
    public:
        /**
         * @brief The depth-stencil format every target carries; a pipeline
         * drawing into a target declares it.
         */
        static constexpr ase::Format depthFormat = ase::FORMAT_DEPTH24_STENCIL8;

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

