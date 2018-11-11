#include "glindexbuffer.h"

#include "dispatcher.h"
#include "buffer.h"

namespace ase
{

GlIndexBuffer::GlIndexBuffer(GlRenderContext& context) :
    size_(0),
    buffer_(context, GL_ELEMENT_ARRAY_BUFFER)
{
}

GlIndexBuffer::~GlIndexBuffer()
{
}

GlBuffer const& GlIndexBuffer::getBuffer() const
{
    return buffer_;
}

size_t GlIndexBuffer::getCount() const
{
    return size_;
}

void GlIndexBuffer::setData(Dispatched d, Buffer const& buffer, Usage usage)
{
    size_ = buffer.getSize() / 2;
    buffer_.setData(d, buffer.data(), buffer.getSize(), usage);
}

} // namespace

