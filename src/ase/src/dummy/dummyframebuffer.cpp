#include "dummyframebuffer.h"

#include "texture.h"
#include "renderbuffer.h"

namespace ase
{

void DummyFramebuffer::setColorTarget(size_t /*index*/, Texture /*texture*/)
{
}

void DummyFramebuffer::unsetColorTarget(size_t /*index*/)
{
}

void DummyFramebuffer::setDepthTarget(Renderbuffer /*buffer*/)
{
}

void DummyFramebuffer::unsetDepthTarget()
{
}

} // namespace ase
