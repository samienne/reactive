#include "glshader.h"

#include "glrendercontext.h"
#include "glplatform.h"

#include "rendercontext.h"

#include "debug.h"

#include <GL/gl.h>

namespace ase
{

GlShader::GlShader(RenderContext& context, std::string const& source,
        GLenum shaderType) :
    platform_(&reinterpret_cast<GlPlatform&>(context.getPlatform()))
{
    try
    {
        auto& glContext = context.getImpl<GlRenderContext>();
        bool failed = false;
        std::string logStr;
        glContext.dispatch([this, &glContext, &logStr, &source, &failed,
                shaderType]()
            {
                GlFunctions const& gl = glContext.getGlFunctions();
                shader_ = gl.glCreateShader(shaderType);
                char const* str = source.c_str();
                gl.glShaderSource(shader_, 1, &str, 0);
                gl.glCompileShader(shader_);

                GLint status;

                gl.glGetShaderiv(shader_, GL_COMPILE_STATUS, &status);
                glFlush();

                if (status == GL_FALSE)
                {
                    failed = true;
                    char* log;
                    GLsizei len;
                    gl.glGetShaderiv(shader_, GL_INFO_LOG_LENGTH, &len);
                    log = new char[len];
                    gl.glGetShaderInfoLog(shader_, len, &len, log);
                    logStr = log;
                    delete [] log;
                }
            });

        glContext.wait();

        if (failed)
        {
            DBG("Failed to compile shader:\n%1\n%2", logStr, source);
            throw std::runtime_error(" shader compilation error:\n"
                    + logStr);
        }
    }
    catch(...)
    {
        destroy();

        throw;
    }

}

GlShader::~GlShader()
{
    destroy();
}

void GlShader::destroy()
{
    if (shader_)
    {
        GLuint shader = shader_;
        shader_ = 0;
        auto& glContext = platform_->getDefaultContext()
            .getImpl<GlRenderContext>();
        platform_->dispatchBackground([&glContext, shader]()
            {
                glContext.getGlFunctions().glDeleteShader(shader);
            });
    }
}

} // namespace

