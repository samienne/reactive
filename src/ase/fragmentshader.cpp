#include "fragmentshader.h"

#include "rendercontext.h"
#include "platform.h"

namespace ase
{

FragmentShader::FragmentShader()
{
}

FragmentShader::FragmentShader(RenderContext& context,
        std::string const& source) :
    deferred_(context.getPlatform().makeFragmentShaderImpl(context, source))
{
}

FragmentShader::~FragmentShader()
{
}

} // namespace

