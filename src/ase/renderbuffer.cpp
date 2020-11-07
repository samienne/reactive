#include "renderbuffer.h"

#include "renderbufferimpl.h"

namespace ase
{

Renderbuffer::Renderbuffer(std::shared_ptr<RenderbufferImpl> impl) :
    deferred_(std::move(impl))
{
}

Format Renderbuffer::getFormat() const
{
    return d()->getFormat();
}

Vector2i Renderbuffer::getSize() const
{
    return d()->getSize();
}

} // namespace ase

