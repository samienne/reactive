#include "vertexshader.h"

#include "rendercontext.h"
#include "platform.h"

namespace ase
{

VertexShader::VertexShader(RenderContext& context, std::string const& source) :
    deferred_(context.getPlatform().makeVertexShaderImpl(context, source))
{
}

VertexShader::~VertexShader()
{
}

} // namespace

