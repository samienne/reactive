#include "gluniformset.h"

#include "uniformbufferrange.h"

namespace ase
{

GlUniformSet::GlUniformSet(GlRenderContext& /*context*/,
        ShaderInterface iface) :
    UniformSetImpl(std::move(iface))
{
}

void GlUniformSet::bindUniformBufferRange(
        int binding,
        UniformBufferRange const& buffer
        )
{
    bindings_.insert_or_assign(static_cast<size_t>(binding), buffer);
    ++version_;
}

GlUniformSet::ConstIterator GlUniformSet::begin() const
{
    return bindings_.begin();
}

GlUniformSet::ConstIterator GlUniformSet::end() const
{
    return bindings_.end();
}

uint64_t GlUniformSet::getVersion() const
{
    return version_;
}

} // namespace ase

