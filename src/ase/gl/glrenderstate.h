#pragma once

#include "vector.h"

#include <GL/gl.h>
#include <GL/glext.h>

namespace ase
{
    class GlRenderContext;
    class CommandBuffer;
    class VertexSpec;
    class UniformBuffer;
    class FramebufferImpl;
    class GlDispatchedContext;
    class GlBaseFramebuffer;
    class GlUniformSet;
    struct GlFunctions;
    struct Dispatched;

    class GlRenderState
    {
    public:
        GlRenderState(GlRenderContext& context, GlDispatchedContext& dispatcher);
        ~GlRenderState();

        void submit(CommandBuffer&& commands);
        void clear(Dispatched, GLbitfield mask);
        void setViewport(Dispatched, Vector2i size);
        void endFrame();

    private:
        // These functions need to be called in dispatched context.
        void pushSpec(Dispatched, GlFunctions const& gl, VertexSpec const& spec,
                std::vector<int>& activeAttribs);
        void dispatchedRenderQueue(Dispatched, GlFunctions const& gl,
                CommandBuffer&& commands);

    private:
        GlRenderContext& context_;
        GlDispatchedContext& dispatcher_;

        // Current state
        Vector2i viewportSize_;
        GlBaseFramebuffer const* boundFramebuffer_ = nullptr;
        GLuint vertexArrayObject_ = 0;
        GLuint boundProgram_ = 0;
        GLuint boundVbo_ = 0;
        GLuint boundIbo_ = 0;
        GLenum srcFactor_ = 0;
        GLenum dstFactor_ = 0;
        GlUniformSet const* boundUniformSet_ = nullptr;
        std::vector<GLint> activeAttribs_;
        std::vector<GLuint> activeTextures_;
        bool enableDepthWrite_ = false;
        bool blendEnabled_ = false;
    };
} // namespace ase

