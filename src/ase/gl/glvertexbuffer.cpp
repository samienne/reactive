#include "glvertexbuffer.h"

#include "buffer.h"
#include "dispatcher.h"

#include "debug.h"

#include <GL/gl.h>

namespace ase
{

GlVertexBuffer::GlVertexBuffer(RenderContext& context) :
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

void GlVertexBuffer::setData(Dispatched, GlRenderContext& context,
        Buffer const& buffer, Usage usage)
{
    //DBG("GlVertexBuffer::setData len: %1", buffer.getSize());
    buffer_.setData(Dispatched(), context, buffer.data(), buffer.getSize(),
            usage);

    size_ = buffer.getSize();
}

GlBuffer const& GlVertexBuffer::getBuffer() const
{
    return buffer_;
}

}// namespace

