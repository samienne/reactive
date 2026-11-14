#include "uniformset.h"

#include "uniformsetimpl.h"
#include "uniformbufferrange.h"

#include <memory>
#include <stdexcept>
#include <string>

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

ShaderInterface const& UniformSet::getInterface() const
{
    return d()->getInterface();
}

void UniformSet::bindUniformBufferRange(
        int binding,
        UniformBufferRange const& buffer
        )
{
    for (auto const& block : getInterface().getBlocks())
    {
        if (block.binding != binding)
            continue;

        if (buffer.size < block.size)
        {
            throw std::invalid_argument("Range of "
                    + std::to_string(buffer.size)
                    + " bytes is smaller than block '" + block.name + "' ("
                    + std::to_string(block.size) + " bytes)");
        }

        d()->bindUniformBufferRange(binding, buffer);
        return;
    }

    throw std::invalid_argument("No block declared at binding "
            + std::to_string(binding));
}

void UniformSet::bindUniformBufferRange(
        std::string_view blockName,
        UniformBufferRange const& buffer
        )
{
    ShaderInterface::Block const* block = getInterface().findBlock(blockName);
    if (!block)
    {
        throw std::invalid_argument("No block '" + std::string(blockName)
                + "' declared");
    }

    bindUniformBufferRange(block->binding, buffer);
}

UniformSetImpl::UniformSetImpl(ShaderInterface iface) :
    iface_(std::move(iface))
{
}

ShaderInterface const& UniformSetImpl::getInterface() const
{
    return iface_;
}

} // namespace ase

