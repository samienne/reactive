#include "glrenderstate.h"

#include "gluniformbuffer.h"
#include "gluniformset.h"
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
#include "gldispatchedcontext.h"
#include "glrendercontext.h"

#include "rendercommand.h"
#include "commandbuffer.h"

#include "debug.h"

#include "systemgl.h"

#include <algorithm>
#include <variant>

namespace ase
{

namespace
{
    bool compareCommand(RenderCommand const& c1, RenderCommand const& c2)
    {
        assert(std::holds_alternative<DrawCommand>(c1) &&
                std::holds_alternative<DrawCommand>(c2));


        DrawCommand const& d1 = std::get<DrawCommand>(c1);
        DrawCommand const& d2 = std::get<DrawCommand>(c2);

        if (d1.getFramebuffer() != d2.getFramebuffer())
        {
            return d1.getFramebuffer() < d2.getFramebuffer();
        }

        GlPipeline const& pipeline1 = d1.getPipeline().getImpl<GlPipeline>();
        GlPipeline const& pipeline2 = d2.getPipeline().getImpl<GlPipeline>();

        if (pipeline1.isBlendEnabled() != pipeline2.isBlendEnabled())
            return pipeline2.isBlendEnabled();

        if (pipeline1.isBlendEnabled())
        {
            // Sort by Z
            if (d1.getZ() != d2.getZ())
                return d1.getZ() < d2.getZ();
        }

        // Sort by textures
        if (d1.getTextures() != d2.getTextures())
            return d1.getTextures() < d2.getTextures();

        // Sort by program
        if (pipeline1.getProgram() != pipeline2.getProgram())
            return pipeline1.getProgram() < pipeline2.getProgram();

        // Sort by VertexBuffer
        if (d1.getVertexBuffer() != d2.getVertexBuffer())
            return d1.getVertexBuffer() < d2.getVertexBuffer();

        if (d1.getIndexBuffer() != d2.getIndexBuffer())
            return d1.getIndexBuffer() < d2.getIndexBuffer();

        // Sort by uniforms
        if (d1.getUniforms() != d2.getUniforms())
            return d1.getUniforms() < d2.getUniforms();

        return d1.getZ() > d2.getZ();
    }
} // anonymous namespace


GlRenderState::GlRenderState(GlRenderContext& context,
        GlDispatchedContext& dispatcher) :
    context_(context),
    dispatcher_(dispatcher)
{
    dispatcher_.dispatch([](GlFunctions const&)
    {
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
    });
}

GlRenderState::~GlRenderState()
{
    if (vertexArrayObject_)
    {
        dispatcher_.dispatch([this](GlFunctions const& gl)
        {
            gl.glDeleteVertexArrays(1, &vertexArrayObject_);
            vertexArrayObject_ = 0;
        });

        dispatcher_.wait();
    }
}

void GlRenderState::submit(CommandBuffer&& commands)
{
    dispatcher_.dispatch([this, commands=std::move(commands)]
            (GlFunctions const& gl) mutable
    {
        auto b = commands.begin();
        auto e = b;

        while (e != commands.end())
        {
            auto const& framebuffer = std::get<DrawCommand>(*b).getFramebuffer();
            while (std::holds_alternative<DrawCommand>(*e)
                    && e != commands.end()
                    && framebuffer == std::get<DrawCommand>(*e).getFramebuffer())
            {
                ++e;
            }

            std::sort(commands.begin(), commands.end(), compareCommand);
            b = e;
        }

        dispatchedRenderQueue(Dispatched(), gl, std::move(commands));
    });
}

void GlRenderState::clear(Dispatched, GLbitfield mask)
{
    if (!enableDepthWrite_ && (mask & GL_DEPTH_BUFFER_BIT))
    {
        enableDepthWrite_ = true;
        glDepthMask(GL_TRUE);
    }

    glClear(mask);
}

void GlRenderState::setViewport(Dispatched, Vector2i size)
{
    if (viewportSize_ != size)
    {
        viewportSize_ = size;
        glViewport(0, 0, size[0], size[1]);
    }
}

void GlRenderState::endFrame()
{
    boundUniformSet_ = nullptr;
    boundVbo_ = 0;
    boundIbo_ = 0;
}

void GlRenderState::dispatchedRenderQueue(Dispatched, GlFunctions const& gl,
        CommandBuffer&& commands)
{
    if (vertexArrayObject_ == 0)
    {
        gl.glGenVertexArrays(1, &vertexArrayObject_);
        gl.glBindVertexArray(vertexArrayObject_);
    }

    boundFramebuffer_ = nullptr;
    for (auto i = commands.begin(); i != commands.end(); ++i)
    {
        RenderCommand const& renderCommand = *i;

        DrawCommand const& command = std::get<DrawCommand>(renderCommand);

        GlPipeline const& pipeline = command.getPipeline().getImpl<GlPipeline>();
        Program const& program = pipeline.getProgram();
        GlProgram const& glProgram = program.getImpl<GlProgram>();
        auto& textures = command.getTextures();
        GlVertexBuffer const& vertexBuffer = command.getVertexBuffer()
            .getImpl<GlVertexBuffer>();
        GlBaseFramebuffer const& framebuffer = command.getFramebuffer().getImpl<
            GlBaseFramebuffer>();
        GlUniformSet const& uniformSet = command.getUniforms()
            .getImpl<GlUniformSet>();

        GLuint vbo = 0;
        GLuint ibo = 0;
        size_t count = 0;
        GLenum mode;

        if (boundFramebuffer_ != &framebuffer)
        {
            boundFramebuffer_ = &framebuffer;
            framebuffer.makeCurrent(Dispatched(), context_, gl);
        }

        vbo = vertexBuffer.getBuffer().getBuffer();
        mode = primitiveToGl(pipeline.getVertexSpec().getPrimitiveType());

        if (command.getIndexBuffer().valid())
        {
            GlIndexBuffer const& ib = command.getIndexBuffer()
                ->getImpl<GlIndexBuffer>();

            ibo = ib.getBuffer().getBuffer();
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
            GLuint tex = i->getImpl<GlTexture>().getGlObject();

            if (j == activeTextures_.end() || *j != tex)
            {
                if (j != activeTextures_.end() || tex)
                {
                    gl.glActiveTexture(GL_TEXTURE0 + index);

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

        if (boundProgram_ != glProgram.getGlObject())
        {
            gl.glUseProgram(glProgram.getGlObject());
            boundProgram_ = glProgram.getGlObject();
        }

        if (boundVbo_ != vbo)
        {
            glGetError();
            gl.glBindBuffer(GL_ARRAY_BUFFER, vbo);

            if (vbo)
            {
                pushSpec(Dispatched(), gl, pipeline.getVertexSpec(),
                        activeAttribs_);
            }

            boundVbo_ = vbo;
        }

        if (boundIbo_ != ibo)
        {
            gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
            boundIbo_ = ibo;
        }

        if (boundUniformSet_ != &uniformSet)
        {
            boundUniformSet_ = &uniformSet;

            for (auto const& uniformRange : uniformSet)
            {
                size_t binding = uniformRange.first;
                UniformBufferRange const& range = uniformRange.second;
                GlUniformBuffer const& uniformBuffer = range.buffer.getImpl<
                    GlUniformBuffer>();

                gl.glBindBufferRange(GL_UNIFORM_BUFFER, binding,
                        uniformBuffer.getBuffer().getBuffer(),
                        range.offset,
                        range.size
                        );
            }
        }

        if (ibo)
            glDrawElements(mode, count, GL_UNSIGNED_SHORT, 0);
        else
            glDrawArrays(mode, 0, count);
    }
}

void GlRenderState::pushSpec(Dispatched, GlFunctions const& gl,
        VertexSpec const& spec, std::vector<GLint>& activeAttribs)
{
    size_t stride = spec.getStride();
    std::vector<VertexSpec::Spec> const& specs = spec.getSpecs();
    std::vector<GLint> attribs;
    attribs.reserve(specs.size());

    for (auto i = specs.begin(); i != specs.end(); ++i)
    {
        VertexSpec::Spec const& spec = *i;

        attribs.push_back(spec.attribLoc);
        gl.glVertexAttribPointer(spec.attribLoc, spec.size, typeToGl(spec.type),
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
            gl.glDisableVertexAttribArray(*j);
            ++j;
        }

        if (j == activeAttribs.end() || *i < *j)
            gl.glEnableVertexAttribArray(*i);

        if (j != activeAttribs.end())
            ++j;
    }
    activeAttribs.swap(attribs);
}

} // namespace ase

