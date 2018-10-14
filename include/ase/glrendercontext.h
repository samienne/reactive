#pragma once

#include "glframebuffer.h"
#include "vertexbuffer.h"
#include "indexbuffer.h"

#include "rendercontextimpl.h"
#include "dispatcher.h"
#include "vector.h"

#include <btl/visibility.h>

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
    };

    class BTL_VISIBLE GlRenderContext : public RenderContextImpl
    {
    public:
        GlRenderContext(GlPlatform& platform);
        ~GlRenderContext() override;

        // From RenderContextImpl
        void submit(std::vector<RenderCommand>&& commands) override;
        void flush() override;
        void finish() override;

        GlFunctions const& getGlFunctions() const;

    protected:
        friend class GlProgram;
        friend class GlTexture;
        friend class GlBuffer;
        friend class GlShader;
        friend class GlPlatform;
        void dispatch(std::function<void()>&& func);
        void wait() const;

        // This function need to be called in dispatched context.
        void glInit(Dispatched, GlFunctions const& gl);
        void glDeinit(Dispatched);

        friend class GlRenderTargetObject;
        GlFramebuffer& getSharedFramebuffer(Dispatched);
        void setViewport(Dispatched, Vector2i size);
        void clear(Dispatched, GLbitfield mask);

    private:

        // These functions need to be called in dispatched context.
        void pushSpec(Dispatched, VertexSpec const& spec,
                std::vector<int>& activeAttribs);
        void pushUniforms(Dispatched, UniformBuffer const& uniforms);
        void dispatchedRenderQueue(Dispatched,
                std::vector<RenderCommand> const& commands);

    private:
        GlPlatform& platform_;
        Dispatcher dispatcher_;
        GlFunctions gl_;
        GlFramebuffer sharedFramebuffer_;
        GlFramebuffer const* currentFramebuffer_ = 0;
        RenderTargetImpl const* boundRenderTarget_ = 0;
        Vector2i viewportSize_;

        // Current state
        GLuint boundProgram_ = 0;
        GLuint boundVbo_ = 0;
        GLuint boundIbo_ = 0;
        GLenum srcFactor_ = 0;
        GLenum dstFactor_ = 0;
        size_t uniformHash_ = 0;
        std::vector<GLint> activeAttribs_;
        std::vector<GLuint> activeTextures_;
        VertexBuffer boundVboObject_;
        IndexBuffer voundIboObject_;
        bool enableDepthWrite_ = false;
        bool blendEnabled_ = false;
    };
}

