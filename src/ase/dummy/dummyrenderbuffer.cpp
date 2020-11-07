#include "dummyrenderbuffer.h"

namespace ase
{

DummyRenderBuffer::DummyRenderBuffer(Format format, Vector2i size) :
    format_(format),
    size_(size)
{
}

Format DummyRenderBuffer::getFormat() const
{
    return format_;
}

Vector2i DummyRenderBuffer::getSize() const
{
    return size_;
}

} // namespace ase
