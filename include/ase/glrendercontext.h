#pragma once

#include "glframebuffer.h"

#include "vertexbuffer.h"
#include "indexbuffer.h"
#include "commandbuffer.h"

#include "rendercontextimpl.h"
#include "dispatcher.h"
#include "vector.h"

#include <btl/visibility.h>
#include <btl/option.h>

#include <GL/gl.h>
#include <GL/glext.h>

namespace ase
{
    class VertexSpec;
    class UniformBuffer;
    class RenderCommand;
    class RenderTarget;
    class RenderTargetImpl;
    class GlRenderTargetObject;
    class GlPlatform;
    class GlVertexBuffer;

    struct BTL_VISIBLE GlFunctions
    {
        PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;
        PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
        PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
        PFNGLUNIFORM1FVPROC glUniform1fv = nullptr;
        PFNGLUNIFORM2FVPROC glUniform2fv = nullptr;
        PFNGLUNIFORM3FVPROC glUniform3fv = nullptr;
        PFNGLUNIFORM4FVPROC glUniform4fv = nullptr;
        PFNGLUNIFORM1IVPROC glUniform1iv = nullptr;
        PFNGLUNIFORM2IVPROC glUniform2iv = nullptr;
        PFNGLUNIFORM3IVPROC glUniform3iv = nullptr;
        PFNGLUNIFORM4IVPROC glUniform4iv = nullptr;
        PFNGLUNIFORM1UIVPROC glUniform1uiv = nullptr;
        PFNGLUNIFORM2UIVPROC glUniform2uiv = nullptr;
        PFNGLUNIFORM3UIVPROC glUniform3uiv = nullptr;
        PFNGLUNIFORM4UIVPROC glUniform4uiv = nullptr;
        PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = nullptr;
        PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
        PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
        PFNGLGENBUFFERSPROC glGenBuffers = nullptr;
        PFNGLBUFFERDATAPROC glBufferData = nullptr;
        PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;
        PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
        PFNGLATTACHSHADERPROC glAttachShader = nullptr;
        PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
        PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
        PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
        PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib = nullptr;
        PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = nullptr;
        PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform = nullptr;
        PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
        PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
        PFNGLCREATESHADERPROC glCreateShader = nullptr;
        PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
        PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
        PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
        PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
        PFNGLDELETESHADERPROC glDeleteShader = nullptr;
        PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
        PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = nullptr;
        PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = nullptr;
        PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
        PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = nullptr;
        PFNGLBINDVERTEXARRAYPROC glBindVertexArray = nullptr;
        PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = nullptr;
    };

    class BTL_VISIBLE GlRenderContext : public RenderContextImpl
    {
    public:
        GlRenderContext(GlPlatform& platform);
        ~GlRenderContext() override;

        GlPlatform& getPlatform() const;

        // From RenderContextImpl
        void submit(CommandBuffer&& commands) override;
        void flush() override;
        void finish() override;

        GlFunctions const& getGlFunctions() const;
        GlFramebuffer const& getDefaultFramebuffer() const;

    protected:
        friend class GlProgram;
        friend class GlTexture;
        friend class GlBuffer;
        friend class GlShader;
        friend class GlPlatform;
        friend class GlPipeline;
        friend class GlFramebuffer;

        void dispatch(std::function<void()>&& func);
        void dispatchBg(std::function<void()>&& func);
        void wait() const;
        void waitBg() const;

        // This function need to be called in dispatched context.
        void glInit(Dispatched, GlFunctions const& gl);
        void glDeinit(Dispatched);

        friend class GlRenderTargetObject;
        GlFramebuffer& getSharedFramebuffer(Dispatched);
        void setViewport(Dispatched, Vector2i size);
        void clear(Dispatched, GLbitfield mask);

        // From RenderContextImpl
        std::shared_ptr<ProgramImpl> makeProgramImpl(
                VertexShader const& vertexShader,
                FragmentShader const& fragmentShader) override;

        std::shared_ptr<VertexShaderImpl> makeVertexShaderImpl(
                std::string const& source) override;

        std::shared_ptr<FragmentShaderImpl> makeFragmentShaderImpl(
                std::string const& source) override;

        std::shared_ptr<VertexBufferImpl> makeVertexBufferImpl(
                Buffer const& buffer,
                Usage usage) override;

        std::shared_ptr<IndexBufferImpl> makeIndexBufferImpl(
                Buffer const& buffer,
                Usage usage) override;

        std::shared_ptr<TextureImpl> makeTextureImpl(
                Vector2i const& size,
                Format format,
                Buffer const& buffer) override;

        std::shared_ptr<RenderTargetObjectImpl>
            makeRenderTargetObjectImpl() override;

        std::shared_ptr<PipelineImpl> makePipeline(
                Program program,
                VertexSpec spec) override;

        std::shared_ptr<PipelineImpl> makePipelineWithBlend(
                Program program,
                VertexSpec spec,
                BlendMode srcFactor,
                BlendMode dstFactor) override;

    private:
        // These functions need to be called in dispatched context.
        void pushSpec(Dispatched, VertexSpec const& spec,
                std::vector<int>& activeAttribs);
        void pushUniforms(Dispatched, UniformBuffer const& uniforms);
        void dispatchedRenderQueue(Dispatched, CommandBuffer&& commands);

    private:
        GlPlatform& platform_;
        Dispatcher dispatcher_;
        Dispatcher dispatcherBg_;
        GlFunctions gl_;
        GlFramebuffer defaultFramebuffer_;
        btl::option<GlFramebuffer> sharedFramebuffer_;
        GlFramebuffer const* currentFramebuffer_ = 0;
        RenderTargetImpl const* boundRenderTarget_ = 0;
        Vector2i viewportSize_;

        // Current state
        GLuint vertexArrayObject_ = 0;
        GLuint boundProgram_ = 0;
        GLuint boundVbo_ = 0;
        GLuint boundIbo_ = 0;
        GLenum srcFactor_ = 0;
        GLenum dstFactor_ = 0;
        size_t uniformHash_ = 0;
        std::vector<GLint> activeAttribs_;
        std::vector<GLuint> activeTextures_;
        bool enableDepthWrite_ = false;
        bool blendEnabled_ = false;
    };
}

