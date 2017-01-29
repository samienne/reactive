#include "texture.h"

#include "textureimpl.h"
#include "rendercontext.h"
#include "platform.h"
#include "async.h"

namespace ase
{

Texture::Texture()
{
}

Texture::Texture(RenderContext& context, Vector2i const& size, Format format,
        Buffer const& buffer) :
    Texture(context, size, format, buffer, Async())
{
    context.flush();
}

Texture::Texture(RenderContext& context, Vector2i const& size, Format format,
        Buffer const& buffer, Async /*async*/) :
    deferred_(context.getPlatform().makeTextureImpl(context, size, format,
                buffer))
{
}

Texture::~Texture()
{
}

bool Texture::operator==(Texture const& other) const
{
    return deferred_ == other.deferred_;
}

bool Texture::operator!=(Texture const& other) const
{
    return deferred_ != other.deferred_;
}

bool Texture::operator<=(Texture const& other) const
{
    return deferred_ <= other.deferred_;
}

Format Texture::getFormat()
{
    return format_;
}

Texture::operator bool() const
{
    return d();
}

Vector2i Texture::getSize() const
{
    if (d())
        return d()->getSize();

    return Vector2i(0, 0);
}

} // namespace

