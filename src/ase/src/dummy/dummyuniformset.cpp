#include "dummyuniformset.h"

namespace ase
{

DummyUniformSet::DummyUniformSet(ShaderInterface iface) :
    UniformSetImpl(std::move(iface))
{
}

void DummyUniformSet::bindUniformBufferRange(
        int binding,
        UniformBufferRange const& buffer
        )
{
    bindings_.insert_or_assign(binding, buffer);
}

std::map<int, UniformBufferRange> const& DummyUniformSet::getBindings() const
{
    return bindings_;
}

} // namespace ase
