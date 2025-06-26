#include "dummytexture.h"

namespace ase
{

DummyTexture::DummyTexture(Vector2i size, Format format) :
    size_(size),
    format_(format)
{
}

void DummyTexture::setSize(Vector2i size)
{
    size_ = size;
}

void DummyTexture::setFormat(Format format)
{
    format_ = format;
}

Vector2i DummyTexture::getSize() const
{
    return size_;
}

Format DummyTexture::getFormat() const
{
    return format_;
}

} // namespace ase

