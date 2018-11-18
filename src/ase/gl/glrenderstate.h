#pragma once

#include "vector.h"

#include <GL/gl.h>
#include <GL/glext.h>

namespace ase
{
    class GlRenderContext;
    class Dispatched;
    class GlFunctions;
    class CommandBuffer;
    class VertexSpec;
    class UniformBuffer;
    class RenderTargetImpl;

    class GlRenderState
    {
    public:
        GlRenderState(Dispatched, GlRenderContext& context);
        ~GlRenderState();

        void submit(Dispatched, GlFunctions const& gl,
                CommandBuffer&& commands);
        void clear(Dispatched, GlFunctions const& gl, GLbitfield mask);
        void setViewport(Dispatched, GlFunctions const& gl, Vector2i size);

        void deinit(Dispatched, GlFunctions const& gl);

    private:
        // These functions need to be called in dispatched context.
        void pushSpec(Dispatched, GlFunctions const& gl, VertexSpec const& spec,
                std::vector<int>& activeAttribs);
        void pushUniforms(Dispatched, GlFunctions const& gl,
                UniformBuffer const& uniforms);
        void dispatchedRenderQueue(Dispatched, GlFunctions const& gl,
                CommandBuffer&& commands);

    private:
        GlRenderContext& context_;

        // Current state
        Vector2i viewportSize_;
        RenderTargetImpl const* boundRenderTarget_ = 0;
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
} // namespace ase

