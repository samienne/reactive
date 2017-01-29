#include "indexbuffer.h"

#include "indexbufferimpl.h"
#include "rendercontext.h"
#include "platform.h"
#include "buffer.h"
#include "async.h"

namespace ase
{

IndexBuffer::IndexBuffer()
{
}

IndexBuffer::IndexBuffer(RenderContext& context, Buffer const& buffer,
        Usage usage) :
    deferred_(context.getPlatform().makeIndexBufferImpl(context, buffer,
                usage))
{
    context.flush();
}

IndexBuffer::IndexBuffer(RenderContext& context, Buffer const& buffer,
        Usage usage, Async /*async*/) :
    deferred_(context.getPlatform().makeIndexBufferImpl(context, buffer,
                usage))
{
}

IndexBuffer::~IndexBuffer()
{
}

bool IndexBuffer::isEmpty() const
{
    return true;
}

bool IndexBuffer::operator<(IndexBuffer const& other) const
{
    return d() < other.d();
}

IndexBuffer::operator bool() const
{
    return d();
}

} // namespace

