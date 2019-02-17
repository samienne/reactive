#include "uniformbuffer.h"

#include "uniformbufferimpl.h"

namespace ase
{

UniformBuffer::UniformBuffer(std::shared_ptr<UniformBufferImpl> impl) :
    deferred_(std::move(impl))
{
}

void UniformBuffer::setData(Buffer buffer, Usage usage)
{
    deferred_->setData(std::move(buffer), usage);
}

} // namespace

