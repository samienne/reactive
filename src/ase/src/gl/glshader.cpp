#include "glshader.h"

#include "glrendercontext.h"
#include "glplatform.h"
#include "glfunctions.h"

#include "buffer.h"
#include "rendercontext.h"

#include "debug.h"

#include "systemgl.h"

#include <string>

namespace ase
{

namespace
{
    GLenum stageToGl(ShaderStage stage)
    {
        switch (stage)
        {
            case ShaderStage::Vertex:
                return GL_VERTEX_SHADER;
            case ShaderStage::Fragment:
                return GL_FRAGMENT_SHADER;
        }

        return GL_VERTEX_SHADER;
    }
} // anonymous namespace

GlShader::GlShader(GlRenderContext& context, ShaderStage stage,
        Buffer const& source) :
    context_(context),
    stage_(stage)
{
    try
    {
        bool failed = false;
        std::string logStr;

        context.dispatchBg([this, &logStr, &source, &failed,
                stage](GlFunctions const& gl)
            {
                shader_ = gl.glCreateShader(stageToGl(stage));

                char const* str = source.mapRead<char>();
                GLint const len = static_cast<GLint>(source.getSize());

                gl.glShaderSource(shader_, 1, &str, &len);
                gl.glCompileShader(shader_);

                GLint status;

                gl.glGetShaderiv(shader_, GL_COMPILE_STATUS, &status);
                glFlush();

                if (status == GL_FALSE)
                {
                    GLsizei logLen;
                    gl.glGetShaderiv(shader_, GL_INFO_LOG_LENGTH, &logLen);

                    std::string log(static_cast<size_t>(logLen), '\0');
                    gl.glGetShaderInfoLog(shader_, logLen, &logLen,
                            log.data());
                    log.resize(static_cast<size_t>(logLen));

                    failed = true;
                    logStr = std::move(log);
                }
            });

        context.waitBg();

        if (failed)
        {
            DBG("Failed to compile shader:\n%1\n%2", logStr,
                    std::string(source.mapRead<char>(), source.getSize()));
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

ShaderStage GlShader::getStage() const
{
    return stage_;
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
