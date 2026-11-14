#include "dummyshader.h"

namespace ase
{

DummyShader::DummyShader(ShaderStage stage) :
    stage_(stage)
{
}

ShaderStage DummyShader::getStage() const
{
    return stage_;
}

} // namespace ase
