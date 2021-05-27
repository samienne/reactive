#include "texture.h"

#include "textureimpl.h"
#include "rendercontext.h"
#include "platform.h"
#include "async.h"

namespace ase
{

Texture::Texture(std::shared_ptr<TextureImpl> impl) :
    deferred_(std::move(impl))
{
}

Texture::~Texture()
{
}

bool Texture::operator==(Texture const& rhs) const
{
    return deferred_ == rhs.deferred_;
}

bool Texture::operator!=(Texture const& rhs) const
{
    return deferred_ != rhs.deferred_;
}

bool Texture::operator<=(Texture const& rhs) const
{
    return deferred_ <= rhs.deferred_;
}

bool Texture::operator<(Texture const& rhs) const
{
    return deferred_ < rhs.deferred_;
}

Format Texture::getFormat()
{
    return d()->getFormat();
}

Vector2i Texture::getSize() const
{
    return d()->getSize();
}

} // namespace

