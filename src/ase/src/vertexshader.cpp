#include "vertexshader.h"

#include "rendercontext.h"
#include "platform.h"

namespace ase
{

VertexShader::VertexShader(std::shared_ptr<VertexShaderImpl> impl) :
    deferred_(std::move(impl))
{
}

VertexShader::~VertexShader()
{
}

} // namespace

