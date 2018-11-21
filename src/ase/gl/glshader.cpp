#include "glshader.h"

#include "glrendercontext.h"
#include "glplatform.h"
#include "glfunctions.h"

#include "rendercontext.h"

#include "debug.h"

#include <GL/gl.h>

namespace ase
{

GlShader::GlShader(GlRenderContext& context, std::string const& source,
        GLenum shaderType) :
    context_(context)
{
    try
    {
        bool failed = false;
        std::string logStr;

        context.dispatchBg([this, &logStr, &source, &failed,
                shaderType](GlFunctions const& gl)
            {
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

        context.waitBg();

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

        context_.dispatchBg([shader](GlFunctions const& gl)
            {
                gl.glDeleteShader(shader);
            });
    }
}

} // namespace

