#include "gluniformbuffer.h"
#include "glrendercontext.h"

#include <iostream>

namespace ase
{

GlUniformBuffer::GlUniformBuffer(GlRenderContext& context) :
    context_(context),
    size_(0),
    buffer_(context, GL_UNIFORM_BUFFER)
{
}

GlUniformBuffer::~GlUniformBuffer()
{
    std::cout << "delete uniform buffer: " << buffer_.getBuffer() << std::endl;
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

GlBuffer const& GlUniformBuffer::getBuffer() const
{
    return buffer_;
}

void GlUniformBuffer::setData(Buffer buffer, Usage usage)
{
    context_.dispatchBg([this, buffer=std::move(buffer), usage]
        (GlFunctions const& gl) mutable
        {
            setData(Dispatched(), gl, std::move(buffer), usage);
            glFlush();
        });

    context_.waitBg();
}

} // namespace ase

