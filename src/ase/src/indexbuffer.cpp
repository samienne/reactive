#include "indexbuffer.h"

#include "indexbufferimpl.h"
#include "rendercontext.h"
#include "platform.h"
#include "buffer.h"
#include "async.h"

namespace ase
{

IndexBuffer::IndexBuffer(std::shared_ptr<IndexBufferImpl> impl) :
    deferred_(impl)
{
}

IndexBuffer::~IndexBuffer()
{
}

bool IndexBuffer::operator==(IndexBuffer const& rhs) const
{
    return d() == rhs.d();
}

bool IndexBuffer::operator!=(IndexBuffer const& rhs) const
{
    return !(*this == rhs);
}

bool IndexBuffer::operator<(IndexBuffer const& other) const
{
    return d() < other.d();
}

} // namespace

