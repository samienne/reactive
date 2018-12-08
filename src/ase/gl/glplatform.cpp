#include "glplatform.h"

#include "glprogram.h"
#include "glvertexshader.h"
#include "glfragmentshader.h"
#include "glvertexbuffer.h"
#include "glindexbuffer.h"
#include "gltexture.h"
#include "glframebuffer.h"
#include "glrendercontext.h"
#include "glpipeline.h"

#include "vertexshader.h"
#include "fragmentshader.h"
#include "buffer.h"

#include "debug.h"

namespace ase
{

GlPlatform::GlPlatform()
{
    DBG("GlPlatform size: %1 bytes.", sizeof(GlPlatform));
    DBG("GlProgram size: %1 bytes.", sizeof(GlProgram));
    DBG("GlVertexShader size: %1 bytes.", sizeof(GlVertexShader));
    DBG("GlFragmentShader size: %1 bytes.", sizeof(GlFragmentShader));
    DBG("GlVertexBuffer size: %1 bytes.", sizeof(GlVertexBuffer));
    DBG("GlIndexBuffer size: %1 bytes.", sizeof(GlIndexBuffer));
    DBG("GlRenderContext size: %1 bytes.", sizeof(GlRenderContext));
    DBG("GlFramebuffer size: %1 bytes.", sizeof(GlFramebuffer));
    DBG("GlTexture size: %1 bytes.", sizeof(GlTexture));
}

GlPlatform::~GlPlatform()
{
}

} // namespace ase

