#include "dummyframebuffer.h"

#include "texture.h"
#include "renderbuffer.h"

namespace ase
{

DummyFramebuffer::DummyFramebuffer(Format colorFormat,
        std::optional<Format> depthFormat) :
    colorFormat_(colorFormat),
    depthFormat_(depthFormat)
{
}

void DummyFramebuffer::setColorTarget(size_t index, Texture texture)
{
    if (index == 0)
        colorFormat_ = texture.getFormat();
}

void DummyFramebuffer::setColorTarget(size_t index, Renderbuffer buffer)
{
    if (index == 0)
        colorFormat_ = buffer.getFormat();
}

void DummyFramebuffer::unsetColorTarget(size_t index)
{
    if (index == 0)
        colorFormat_ = FORMAT_UNKNOWN;
}

void DummyFramebuffer::setDepthTarget(Renderbuffer buffer)
{
    depthFormat_ = buffer.getFormat();
}

void DummyFramebuffer::unsetDepthTarget()
{
    depthFormat_.reset();
}

void DummyFramebuffer::setStencilTarget(Renderbuffer /*buffer*/)
{
}

void DummyFramebuffer::unsetStencilTarget()
{
}

Format DummyFramebuffer::getColorFormat() const
{
    return colorFormat_;
}

std::optional<Format> DummyFramebuffer::getDepthFormat() const
{
    return depthFormat_;
}

} // namespace ase
