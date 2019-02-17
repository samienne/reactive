#include "glvertexshader.h"

#include "glplatform.h"

#include "debug.h"

#include <GL/gl.h>

namespace ase
{

GlVertexShader::GlVertexShader(GlRenderContext& context,
        std::string const& source) :
    shader_(context, source, GL_VERTEX_SHADER)
{
}

GlVertexShader::~GlVertexShader()
{
}

} // namespace

