#include "fragmentshader.h"

#include "rendercontext.h"
#include "platform.h"

namespace ase
{

FragmentShader::FragmentShader(std::shared_ptr<FragmentShaderImpl> impl) :
    deferred_(std::move(impl))
{
}

FragmentShader::~FragmentShader()
{
}

} // namespace

