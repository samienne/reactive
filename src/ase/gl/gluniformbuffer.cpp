#include "gluniformbuffer.h"

#include "dispatcher.h"

namespace ase
{

GlUniformBuffer::GlUniformBuffer(GlRenderContext& context) :
    size_(0),
    buffer_(context, GL_UNIFORM_BUFFER)
{
}

size_t GlUniformBuffer::getSize()
{
    return size_;
}

void GlUniformBuffer::setData(Dispatched d, GlFunctions const& gl,
        Buffer buffer, Usage usage)
{
    buffer_.setData(d, gl, buffer.data(), buffer.getSize(), usage);
    size_ = buffer.getSize();
}

GlBuffer const GlUniformBuffer::getBuffer() const
{
    return buffer_;
}

} // namespace ase

