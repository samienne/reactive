#include "glrendercontext.h"

#include "glplatform.h"
#include "glprogram.h"
#include "glfragmentshader.h"
#include "glvertexshader.h"
#include "glvertexbuffer.h"
#include "glindexbuffer.h"
#include "gltexture.h"
#include "glerror.h"
#include "gltype.h"
#include "glblendmode.h"
#include "glpipeline.h"
#include "glrendertargetobject.h"

#include "commandbuffer.h"
#include "rendercommand.h"
#include "rendertarget.h"
#include "rendertargetimpl.h"
#include "vertexshader.h"
#include "fragmentshader.h"
#include "buffer.h"
#include "debug.h"


#include <GL/gl.h>

#include <algorithm>
#include <functional>

namespace ase
{

GlRenderContext::GlRenderContext(GlPlatform& platform) :
    platform_(platform),
    dispatcher_(),
    objectManager_(*this),
    defaultFramebuffer_(*this)
{
}

GlRenderContext::~GlRenderContext()
{
}

GlPlatform& GlRenderContext::getPlatform() const
{
    return platform_;
}

void GlRenderContext::submit(CommandBuffer&& commands)
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

GlFramebuffer const& GlRenderContext::getDefaultFramebuffer() const
{
    return defaultFramebuffer_;
}

void GlRenderContext::dispatch(std::function<void()>&& func)
{
    dispatcher_.run(std::move(func));
}

void GlRenderContext::dispatchBg(std::function<void()>&& func)
{
    dispatcherBg_.run(std::move(func));
}

void GlRenderContext::wait() const
{
    dispatcher_.wait();
}

void GlRenderContext::waitBg() const
{
    dispatcherBg_.wait();
}

void GlRenderContext::glInit(Dispatched, GlFunctions const& gl)
{
    gl_ = gl;

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

    sharedFramebuffer_ = btl::just(GlFramebuffer(Dispatched(), *this));
}

void GlRenderContext::glDeinit(Dispatched)
{
    if (vertexArrayObject_)
    {
        gl_.glDeleteVertexArrays(1, &vertexArrayObject_);
        vertexArrayObject_ = 0;
    }

    sharedFramebuffer_->destroy(Dispatched(), *this);
}

GlFramebuffer& GlRenderContext::getSharedFramebuffer(Dispatched)
{
    return *sharedFramebuffer_;
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

std::shared_ptr<ProgramImpl> GlRenderContext::makeProgramImpl(
        VertexShader const& vertexShader,
        FragmentShader const& fragmentShader)
{
    return objectManager_.makeProgram(vertexShader, fragmentShader);
}

std::shared_ptr<VertexShaderImpl> GlRenderContext::makeVertexShaderImpl(
        std::string const& source)
{
    return objectManager_.makeVertexShader(source);
}

std::shared_ptr<FragmentShaderImpl> GlRenderContext::makeFragmentShaderImpl(
        std::string const& source)
{
    return objectManager_.makeFragmentShader(source);
}

std::shared_ptr<VertexBufferImpl> GlRenderContext::makeVertexBufferImpl(
        Buffer const& buffer, Usage usage)
{
    return objectManager_.makeVertexBuffer(std::move(buffer), usage);
}

std::shared_ptr<IndexBufferImpl> GlRenderContext::makeIndexBufferImpl(
        Buffer const& buffer, Usage usage)
{
    return objectManager_.makeIndexBuffer(std::move(buffer), usage);
}

std::shared_ptr<TextureImpl> GlRenderContext::makeTextureImpl(
        Vector2i const& size, Format format, Buffer const& buffer)
{
    return objectManager_.makeTexture(size, format, std::move(buffer));
}

std::shared_ptr<RenderTargetObjectImpl> GlRenderContext::makeRenderTargetObjectImpl()
{
    return objectManager_.makeRenderTargetObject();
}

std::shared_ptr<PipelineImpl> GlRenderContext::makePipeline(
        Program program,
        VertexSpec spec)
{
    return objectManager_.makePipeline(std::move(program), std::move(spec));
}

std::shared_ptr<PipelineImpl> GlRenderContext::makePipelineWithBlend(
        Program program,
        VertexSpec spec,
        BlendMode srcFactor,
        BlendMode dstFactor)
{
    return objectManager_.makePipelineWithBlend(std::move(program),
            std::move(spec), srcFactor, dstFactor);
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

void GlRenderContext::dispatchedRenderQueue(Dispatched, CommandBuffer&& commands)
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
        GlVertexBuffer const& vertexBuffer = command.getVertexBuffer()
            .getImpl<GlVertexBuffer>();

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

        vbo = vertexBuffer.getBuffer().buffer_;
        mode = primitiveToGl(pipeline.getVertexSpec().getPrimitiveType());

        if (command.getIndexBuffer().valid())
        {
            GlIndexBuffer const& ib = command.getIndexBuffer()
                ->getImpl<GlIndexBuffer>();

            ibo = ib.getBuffer().buffer_;
            count = ib.getCount();
        }
        else
            count = vertexBuffer.getSize() / pipeline.getVertexSpec().getStride();

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
            GLuint tex = i->getImpl<GlTexture>().texture_;

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

        if (boundVbo_ != vbo)
        {
            glGetError();
            gl_.glBindBuffer(GL_ARRAY_BUFFER, vbo);

            if (vbo)
                pushSpec(Dispatched(), pipeline.getVertexSpec(), activeAttribs_);

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

