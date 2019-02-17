#include "framebuffer.h"

#include "renderbuffer.h"
#include "framebufferimpl.h"
#include "texture.h"

namespace ase
{

Framebuffer::Framebuffer(std::shared_ptr<FramebufferImpl> impl) :
    deferred_(std::move(impl))
{
}

bool Framebuffer::operator==(Framebuffer const& other) const
{
    return d() == other.d();
}

bool Framebuffer::operator!=(Framebuffer const& other) const
{
    return d() != other.d();
}

bool Framebuffer::operator<(Framebuffer const& other) const
{
    return d() < other.d();
}

void Framebuffer::setColorTarget(size_t index, Texture texture)
{
    d()->setColorTarget(index, std::move(texture));
}

void Framebuffer::setColorTarget(size_t index, Renderbuffer buffer)
{
    d()->setColorTarget(index, std::move(buffer));
}

void Framebuffer::unsetColorTarget(size_t index)
{
    d()->unsetColorTarget(index);
}

void Framebuffer::setDepthTarget(Renderbuffer buffer)
{
    d()->setDepthTarget(std::move(buffer));
}

void Framebuffer::unsetDepthTarget()
{
    d()->unsetDepthTarget();
}

void Framebuffer::setStencilTarget(Renderbuffer buffer)
{
    d()->setStencilTarget(std::move(buffer));
}

void Framebuffer::unsetStencilTarget()
{
    d()->unsetStencilTarget();
}

void Framebuffer::clear()
{
    d()->clear();
}

} // namespace ase

