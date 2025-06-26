#include "uniformset.h"

#include "uniformsetimpl.h"

#include <memory>

namespace ase
{

UniformSet::UniformSet(std::shared_ptr<UniformSetImpl> impl) :
    deferred_(std::move(impl))
{
}

bool UniformSet::operator==(UniformSet const& rhs) const
{
    return deferred_ == rhs.deferred_;
}

bool UniformSet::operator!=(UniformSet const& rhs) const
{
    return deferred_ != rhs.deferred_;
}

bool UniformSet::operator<(UniformSet const& rhs) const
{
    return deferred_ < rhs.deferred_;
}

bool UniformSet::operator>(UniformSet const& rhs) const
{
    return deferred_ > rhs.deferred_;
}

void UniformSet::bindUniformBufferRange(
        int binding,
        UniformBufferRange const& buffer
        )
{
    d()->bindUniformBufferRange(binding, std::move(buffer));
}

} // namespace ase

