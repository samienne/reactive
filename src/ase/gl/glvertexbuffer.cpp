#include "glvertexbuffer.h"

#include "buffer.h"
#include "dispatcher.h"

#include "debug.h"

#include <GL/gl.h>

namespace ase
{

GlVertexBuffer::GlVertexBuffer(GlRenderContext& context) :
    buffer_(context, GL_ARRAY_BUFFER)
{
}

GlVertexBuffer::~GlVertexBuffer()
{
}

size_t GlVertexBuffer::getSize() const
{
    return size_;
}

void GlVertexBuffer::setData(Dispatched, GlFunctions const& gl,
        Buffer const& buffer, Usage usage)
{
    //DBG("GlVertexBuffer::setData len: %1", buffer.getSize());
    buffer_.setData(Dispatched(), gl, buffer.data(), buffer.getSize(), usage);

    size_ = buffer.getSize();
}

GlBuffer const& GlVertexBuffer::getBuffer() const
{
    return buffer_;
}

}// namespace

