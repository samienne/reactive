#include "glfragmentshader.h"

namespace ase
{

GlFragmentShader::GlFragmentShader(RenderContext& context,
        std::string const& source) :
    shader_(context, source, GL_FRAGMENT_SHADER)
{
}

GlFragmentShader::~GlFragmentShader()
{
}

} // namespace

