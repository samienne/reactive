#include "gluniformset.h"

#include "uniformbufferrange.h"

namespace ase
{

GlUniformSet::GlUniformSet(GlRenderContext& /*context*/)
{
}

void GlUniformSet::bindUniformBufferRange(
        int binding,
        UniformBufferRange const& buffer
        )
{
    bindings_.insert(std::make_pair(binding, buffer));
}

GlUniformSet::ConstIterator GlUniformSet::begin() const
{
    return bindings_.begin();
}

GlUniformSet::ConstIterator GlUniformSet::end() const
{
    return bindings_.end();
}

} // namespace ase

