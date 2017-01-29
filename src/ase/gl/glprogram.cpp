#include "glprogram.h"

#include "glvertexshader.h"
#include "glfragmentshader.h"
#include "glrendercontext.h"
#include "glplatform.h"
#include "gltype.h"

#include "rendercontext.h"

#include "debug.h"

namespace ase
{

GlProgram::GlProgram(RenderContext& context, GlVertexShader const& vertexShader,
    GlFragmentShader const& fragmentShader) :
    platform_(&reinterpret_cast<GlPlatform&>(context.getPlatform())),
    program_(0)
{
    try
    {
        bool failed = false;
        std::string logStr;
        auto& glContext = context.getImpl<GlRenderContext>();
        glContext.dispatch([&, this]()
            {
                auto& gl = glContext.getGlFunctions();
                program_ = gl.glCreateProgram();
                gl.glAttachShader(program_, vertexShader.shader_.shader_);
                gl.glAttachShader(program_, fragmentShader.shader_.shader_);

                gl.glLinkProgram(program_);

                GLint status;
                gl.glGetProgramiv(program_, GL_LINK_STATUS, &status);
                glFlush();
                if (status == GL_FALSE)
                {
                    failed = true;
                    char* log;
                    GLsizei len;
                    gl.glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &len);
                    log = new char[len];
                    gl.glGetProgramInfoLog(program_, len, &len, log);
                    logStr = log;
                    delete [] log;
                }

                programIntrospection(gl);
            });
        context.getImpl<GlRenderContext>().wait();

        if (failed)
        {
            DBG("Failed to link program: %1", logStr);
            throw std::runtime_error("Unable to link shader program:\n"
                    + logStr);
        }
    }
    catch (...)
    {
        destroy();
        throw;
    }
}

GlProgram::~GlProgram()
{
    destroy();
}

int GlProgram::getUniformLocation(std::string const& name) const
{
    auto i = uniformLocations_.find(name);

    if (i == uniformLocations_.end())
        return -1;

    return i->second;
}

int GlProgram::getAttribLocation(std::string const& name) const
{
    auto i = attribLocations_.find(name);

    if (i == attribLocations_.end())
        return -1;

    return i->second;
}

void GlProgram::destroy()
{
    if (platform_ && program_)
    {
        GLuint program = program_;
        auto& glContext = platform_->getDefaultContext()
            .getImpl<GlRenderContext>();
        platform_->dispatchBackground([&glContext, program]()
            {
                glContext.getGlFunctions().glDeleteProgram(program);
            });
    }
}

void GlProgram::programIntrospection(GlFunctions const& gl)
{
    GLint count;
    gl.glGetProgramiv(program_, GL_ACTIVE_ATTRIBUTES, &count);

    char name[256];
    GLsizei len;
    GLint size;
    GLenum type;

    for (int i = 0; i < count; ++i)
    {
        gl.glGetActiveAttrib(program_, i, 256, &len, &size, &type, name);
        attribLocations_[name] = gl.glGetAttribLocation(program_, name);
        /*DBG("%1 attribute %2 %3;", attribLocations_[name],
                glTypeToString(type), name);*/
    }

    gl.glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &count);

    for (int i = 0; i < count; ++i)
    {
        gl.glGetActiveUniform(program_, i, 256, &len, &size, &type, name);
        //DBG("uniform %1 %2;", glTypeToString(type), name);
        uniformLocations_[name] = gl.glGetUniformLocation(program_, name);
        //DBG("Location: %1", uniformLocations_[name]);
    }
}

} // namespace

