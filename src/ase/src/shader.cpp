#include "shader.h"

#include "shaderimpl.h"

namespace ase
{

Shader::Shader(std::shared_ptr<ShaderImpl> impl) :
    deferred_(std::move(impl))
{
}

Shader::~Shader()
{
}

ShaderStage Shader::getStage() const
{
    return d()->getStage();
}

} // namespace ase
