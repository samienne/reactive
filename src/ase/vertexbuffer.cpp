#include "vertexbuffer.h"

#include "vertexbufferimpl.h"
#include "rendercontext.h"
#include "platform.h"
#include "aabb.h"
#include "async.h"
#include "buffer.h"

namespace ase
{

VertexBuffer::VertexBuffer()
{
}

VertexBuffer::VertexBuffer(RenderContext& context, Buffer const& buffer,
        Usage usage) :
    deferred_(context.getPlatform().makeVertexBufferImpl(context, buffer,
                usage))
{
    context.flush();
}

VertexBuffer::VertexBuffer(RenderContext& context, Buffer const& buffer,
        Usage usage, Async /*async*/) :
    deferred_(context.getPlatform().makeVertexBufferImpl(context, buffer,
                usage))
{
}

VertexBuffer::~VertexBuffer()
{
}

bool VertexBuffer::operator==(VertexBuffer const& other) const
{
    return d() == other.d();
}

bool VertexBuffer::operator!=(VertexBuffer const& other) const
{
    return d() != other.d();
}

bool VertexBuffer::operator<(VertexBuffer const& other) const
{
    return d() < other.d();
}

/*NamedVertexSpec const& VertexBuffer::getSpec() const
{
    return spec_;
}*/

VertexBuffer::operator bool() const
{
    return d();
}

} // namespace

