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

void GlIndexBuffer::setData(Dispatched d, GlFunctions const& gl,
        Buffer const& buffer, Usage usage)
{
    size_ = buffer.getSize() / sizeof(uint32_t);
    buffer_.setData(d, gl, buffer.data(), buffer.getSize(), usage);
}

} // namespace

