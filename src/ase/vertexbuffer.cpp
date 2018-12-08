#include "vertexbuffer.h"

#include "vertexbufferimpl.h"
#include "rendercontext.h"
#include "platform.h"
#include "aabb.h"
#include "async.h"
#include "buffer.h"

namespace ase
{

VertexBuffer::VertexBuffer(std::shared_ptr<VertexBufferImpl> impl) :
    deferred_(std::move(impl))
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

} // namespace

