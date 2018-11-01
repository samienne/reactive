#include "glrendercontext.h"

#include "glplatform.h"

#include "glprogram.h"
#include "glvertexbuffer.h"
#include "glindexbuffer.h"
#include "gltexture.h"
#include "glerror.h"
#include "gltype.h"
#include "glblendmode.h"
#include "glpipeline.h"

#include "renderqueue.h"
#include "rendercommand.h"
#include "rendertarget.h"
#include "rendertargetimpl.h"
#include "glrendertargetobject.h"
#include "debug.h"

#include <GL/gl.h>

#include <algorithm>
#include <functional>

namespace ase
{

GlRenderContext::GlRenderContext(GlPlatform& platform) :
    platform_(platform),
    dispatcher_()
{
}

GlRenderContext::~GlRenderContext()
{
}

void GlRenderContext::submit(RenderQueue&& commands)
{
    auto compare = [](RenderCommand const& c1, RenderCommand const& c2)
    {
        if (c1.getRenderTarget() != c2.getRenderTarget())
        {
            return c1.getRenderTarget() < c2.getRenderTarget();
        }

        GlPipeline const& pipeline1 = c1.getPipeline().getImpl<GlPipeline>();
        GlPipeline const& pipeline2 = c2.getPipeline().getImpl<GlPipeline>();

        if (pipeline1.isBlendEnabled() != pipeline2.isBlendEnabled())
            return pipeline2.isBlendEnabled();

        if (pipeline1.isBlendEnabled())
        {
            // Sort by Z
            if (c1.getZ() != c2.getZ())
                return c1.getZ() < c2.getZ();
        }

        // Sort by textures
        if (c1.getTextures() != c2.getTextures())
            return c1.getTextures() < c2.getTextures();

        // Sort by program
        if (pipeline1.getProgram() != pipeline2.getProgram())
            return pipeline1.getProgram() < pipeline2.getProgram();

        // Sort by VertexBuffer
        if (c1.getVertexBuffer() != c2.getVertexBuffer())
            return c1.getVertexBuffer() < c2.getVertexBuffer();

        if (c1.getIndexBuffer() != c2.getIndexBuffer())
            return c1.getIndexBuffer() < c2.getIndexBuffer();

        // Sort by uniforms

        return c1.getZ() > c2.getZ();
    };

    dispatch([this, queue=std::move(commands), compare]() mutable
        {
            auto b = queue.begin();
            auto e = b;

            while (e != queue.end())
            {
                auto const& target = b->getRenderTarget();
                while (e != queue.end() && target == e->getRenderTarget())
                    ++e;

                std::sort(queue.begin(), queue.end(), compare);
                b = e;
            }

            dispatchedRenderQueue(Dispatched(), std::move(queue));
        });
}

void GlRenderContext::flush()
{
    wait();
}

void GlRenderContext::finish()
{
    dispatch([]()
            {
                glFinish();
            });
    wait();
}

GlFunctions const& GlRenderContext::getGlFunctions() const
{
    return gl_;
}

void GlRenderContext::dispatch(std::function<void()>&& func)
{
    dispatcher_.run(std::move(func));
}

void GlRenderContext::wait() const
{
    dispatcher_.wait();
}

void GlRenderContext::glInit(Dispatched, GlFunctions const& gl)
{
    gl_ = gl;

    //glEnableClientState(GL_VERTEX_ARRAY);
    glEnable(GL_DEPTH_TEST);

    GLint srgbEnabled = 0;
#ifdef GL_EXT_framebuffer_sRGB
    GLenum err;
    glGetError();

    glGetIntegerv(GL_FRAMEBUFFER_SRGB_CAPABLE_EXT, &srgbEnabled);
    err = glGetError();
    if (err != GL_NO_ERROR)
    {
        DBG("GlRenderContext: failed to read sRGB status, disabling: %1",
                glErrorToString(err));
        srgbEnabled = 0;
    }
    else
        srgbEnabled = 1;

    if (srgbEnabled)
    {
        glEnable(GL_FRAMEBUFFER_SRGB_EXT);
        DBG("GlRenderContext: sRGB surfaces enabled.");
    }
    else
    {
        glDisable(GL_FRAMEBUFFER_SRGB_EXT);
        DBG("GlRenderContext: sRGB surfaces disabled.");
    }
#endif

#ifdef GLX_EXT_swap_control
    //glXSwapIntervalEXT(dpy, glxWin_, 1);
    //glXSwapIntervalSGI(1);
#endif

    sharedFramebuffer_ = GlFramebuffer(Dispatched(), platform_, *this);
}

void GlRenderContext::glDeinit(Dispatched)
{
    if (vertexArrayObject_)
    {
        gl_.glDeleteVertexArrays(1, &vertexArrayObject_);
        vertexArrayObject_ = 0;
    }

    sharedFramebuffer_.destroy(Dispatched(), *this);
}

GlFramebuffer& GlRenderContext::getSharedFramebuffer(Dispatched)
{
    return sharedFramebuffer_;
}

void GlRenderContext::setViewport(Dispatched, Vector2i size)
{
    //if (viewportSize_ != size)
    {
        viewportSize_ = size;
        glViewport(0, 0, size[0], size[1]);
    }
}

void GlRenderContext::clear(Dispatched, GLbitfield mask)
{
    if (!enableDepthWrite_ && mask & GL_DEPTH_BUFFER_BIT)
    {
        enableDepthWrite_ = true;
        glDepthMask(GL_TRUE);
    }

    glClear(mask);
}

void GlRenderContext::pushSpec(Dispatched, VertexSpec const& spec,
        std::vector<GLint>& activeAttribs)
{
    size_t stride = spec.getStride();
    std::vector<VertexSpec::Spec> const& specs = spec.getSpecs();
    std::vector<GLint> attribs;
    attribs.reserve(specs.size());
    for (auto i = specs.begin(); i != specs.end(); ++i)
    {
        VertexSpec::Spec const& spec = *i;

        attribs.push_back(spec.attribLoc);
        glGetError();
        gl_.glVertexAttribPointer(spec.attribLoc, spec.size, typeToGl(spec.type),
                spec.normalized, stride, (void*)spec.pointer);

        /*DBG("attrib: %1 %2 %3 %4 %5 %6", spec.attribLoc, spec.size,
                glTypeToString(typeToGl(spec.type)), spec.normalized,
                stride, spec.pointer);*/
    }

    std::sort(attribs.begin(), attribs.end());

    auto j = activeAttribs.begin();

    for (auto i = attribs.begin(); i != attribs.end(); ++i)
    {
        while (j != activeAttribs.end() && *j < *i)
        {
            gl_.glDisableVertexAttribArray(*j);
            ++j;
        }

        if (j == activeAttribs.end() || *i < *j)
            gl_.glEnableVertexAttribArray(*i);

        if (j != activeAttribs.end())
            ++j;
    }
    activeAttribs.swap(attribs);
}

void GlRenderContext::pushUniforms(Dispatched, UniformBuffer const& buffer)
{
    for (auto&& uniform : buffer)
    {
        switch (uniform.getType())
        {
        case UniformType::uniform1fv:
            gl_.glUniform1fv(uniform.getLocation(), uniform.getCount(),
                    ((GLfloat*)uniform.getData()));
            break;
        case UniformType::uniform2fv:
            gl_.glUniform2fv(uniform.getLocation(), uniform.getCount(),
                    ((GLfloat*)uniform.getData()));
            break;
        case UniformType::uniform3fv:
            gl_.glUniform3fv(uniform.getLocation(), uniform.getCount(),
                    ((GLfloat*)uniform.getData()));
            break;
        case UniformType::uniform4fv:
            gl_.glUniform4fv(uniform.getLocation(), uniform.getCount(),
                    ((GLfloat*)uniform.getData()));
            break;
        case UniformType::uniform1iv:
            gl_.glUniform1iv(uniform.getLocation(), uniform.getCount(),
                    ((GLint*)uniform.getData()));
            break;
        case UniformType::uniform2iv:
            gl_.glUniform1iv(uniform.getLocation(), uniform.getCount(),
                    ((GLint*)uniform.getData()));
            break;
        case UniformType::uniform3iv:
            gl_.glUniform1iv(uniform.getLocation(), uniform.getCount(),
                    ((GLint*)uniform.getData()));
            break;
        case UniformType::uniform4iv:
            gl_.glUniform1iv(uniform.getLocation(), uniform.getCount(),
                    ((GLint*)uniform.getData()));
            break;
        case UniformType::uniform1uiv:
            gl_.glUniform1uiv(uniform.getLocation(), uniform.getCount(),
                    ((GLuint*)uniform.getData()));
            break;
        case UniformType::uniform2uiv:
            gl_.glUniform1uiv(uniform.getLocation(), uniform.getCount(),
                    ((GLuint*)uniform.getData()));
            break;
        case UniformType::uniform3uiv:
            gl_.glUniform1uiv(uniform.getLocation(), uniform.getCount(),
                    ((GLuint*)uniform.getData()));
            break;
        case UniformType::uniform4uiv:
            gl_.glUniform1uiv(uniform.getLocation(), uniform.getCount(),
                    ((GLuint*)uniform.getData()));
            break;
        case UniformType::uniformMatrix4fv:
            gl_.glUniformMatrix4fv(uniform.getLocation(), uniform.getCount(),
                    false, ((GLfloat*)uniform.getData()));
            break;
        }
    }
}

void GlRenderContext::dispatchedRenderQueue(Dispatched, RenderQueue&& commands)
{
    if (vertexArrayObject_ == 0)
    {
        gl_.glGenVertexArrays(1, &vertexArrayObject_);
        gl_.glBindVertexArray(vertexArrayObject_);
    }

    boundRenderTarget_ = nullptr;
    for (auto i = commands.begin(); i != commands.end(); ++i)
    {
        RenderCommand const& command = *i;
        GlPipeline const& pipeline = command.getPipeline().getImpl<GlPipeline>();
        Program const& program = pipeline.getProgram();
        GlProgram const& glProgram = program.getImpl<GlProgram>();
        auto& textures = command.getTextures();
        GLuint vbo = 0;
        GLuint ibo = 0;
        size_t count = 0;
        GLenum mode;

        if (!command.getVertexBuffer())
            continue;

        if (boundRenderTarget_ !=
                &command.getRenderTarget().getImpl<RenderTargetImpl>())
        {
            boundRenderTarget_ = &command.getRenderTarget().getImpl<RenderTargetImpl>();
            boundRenderTarget_->makeCurrent(Dispatched(), *this);
        }

        GlVertexBuffer const& vb = command.getVertexBuffer(
                ).getImpl<GlVertexBuffer>();
        vbo = vb.getBuffer().buffer_;
        mode = primitiveToGl(pipeline.getVertexSpec().getPrimitiveType());

        if (command.getIndexBuffer())
        {
            GlIndexBuffer const& ib = command.getIndexBuffer(
                    ).getImpl<GlIndexBuffer>();
            ibo = command.getIndexBuffer().getImpl<GlIndexBuffer>(
                    ).getBuffer().buffer_;
            count = ib.getCount();
        }
        else
            count = vb.getSize() / pipeline.getVertexSpec().getStride();

        // Set blend
        if (blendEnabled_ != pipeline.isBlendEnabled())
        {
            if (pipeline.isBlendEnabled())
            {
                if (enableDepthWrite_)
                {
                    glDepthMask(GL_FALSE);
                    enableDepthWrite_ = false;
                }
                glEnable(GL_BLEND);
            }
            else
            {
                if (!enableDepthWrite_)
                {
                    glDepthMask(GL_TRUE);
                    enableDepthWrite_ = true;
                }
                glDisable(GL_BLEND);
            }

            if (pipeline.isBlendEnabled() &&
                    (srcFactor_ != blendModeToGl(pipeline.getSrcFactor())
                     || dstFactor_ != blendModeToGl(pipeline.getDstFactor())))
            {
                srcFactor_ = blendModeToGl(pipeline.getSrcFactor());
                dstFactor_ = blendModeToGl(pipeline.getDstFactor());
                glBlendFunc(srcFactor_, dstFactor_);
            }

            blendEnabled_ = pipeline.isBlendEnabled();
        }

        // Set Textures
        int index = 0;
        activeTextures_.reserve(textures.size());
        auto j = activeTextures_.begin();
        for (auto i = textures.begin(); i != textures.end(); ++i)
        {
            GLuint tex = 0;
            if (*i)
                tex = i->getImpl<GlTexture>().texture_;

            if (j == activeTextures_.end() || *j != tex)
            {
                if (j != activeTextures_.end() || tex)
                {
                    glActiveTexture(GL_TEXTURE0 + index);

                    glBindTexture(GL_TEXTURE_2D, tex);
                }

                if (j == activeTextures_.end())
                    activeTextures_.push_back(tex);
                else
                {
                    *j = tex;
                    ++j;
                }
            }

            ++index;
        }

        if (boundProgram_ != glProgram.program_)
        {
            gl_.glUseProgram(glProgram.program_);
            boundProgram_ = glProgram.program_;
            uniformHash_ = 0u;
        }

        if (boundVboObject_ != command.getVertexBuffer())
        {
            glGetError();
            gl_.glBindBuffer(GL_ARRAY_BUFFER, vbo);

            if (vbo)
            {
                GlPipeline const& pipeline = command.getPipeline()
                    .getImpl<GlPipeline>();
                pushSpec(Dispatched(), pipeline.getVertexSpec(), activeAttribs_);
            }

            boundVbo_ = vbo;
        }

        if (boundIbo_ != ibo)
        {
            gl_.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
            boundIbo_ = ibo;
        }

        if (uniformHash_ != command.getUniforms().getHash())
        {
            pushUniforms(Dispatched(), command.getUniforms());
            uniformHash_ = command.getUniforms().getHash();
        }

        if (ibo)
            glDrawElements(mode, count, GL_UNSIGNED_SHORT, 0);
        else
            glDrawArrays(mode, 0, count);
    }
}

} // namespace

