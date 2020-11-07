#include "targetimage.h"

#include <ase/buffer.h>

namespace avg
{

TargetImage::TargetImage(ase::RenderContext& context, Vector2i size,
        ase::Format format) :
    texture_(context.makeTexture(size, format, ase::Buffer())),
    depthbuffer_(context.makeRenderbuffer(size, ase::FORMAT_DEPTH24_STENCIL8)),
    framebuffer_(context.makeFramebuffer())
{
}

Vector2i TargetImage::getSize() const
{
    return texture_.getSize();
}

ase::Texture& TargetImage::getTexture()
{
    return texture_;
}

ase::Framebuffer& TargetImage::getFramebuffer()
{
    return framebuffer_;
}

} // namespace avg

