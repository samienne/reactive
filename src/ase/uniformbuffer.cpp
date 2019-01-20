#include "uniformbuffer.h"

namespace ase
{

UniformBuffer::UniformBuffer(std::shared_ptr<UniformBufferImpl> impl) :
    deferred_(std::move(impl))
{
}

} // namespace

