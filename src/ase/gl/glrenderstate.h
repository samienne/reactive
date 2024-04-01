#pragma once

#include "vector.h"

#include "asevisibility.h"

#include "systemgl.h"

#include <btl/future/promise.h>

namespace ase
{
    class Window;
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

    class ASE_EXPORT GlRenderState
    {
    public:
        GlRenderState(GlDispatchedContext& dispatcher,
                std::function<void(Dispatched, Window&)> presentCallback);
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
        void checkFences(Dispatched, GlFunctions const& gl);

    private:
        GlDispatchedContext& dispatcher_;
        std::function<void(Dispatched, Window&)> presentCallback_;

        // Active sync objects
        struct WaitingFence
        {
            GLsync sync;
            btl::future::Promise<> promise;
        };

        std::vector<WaitingFence> fences_;

        // Current state
        Vector2i viewportSize_ = Vector2i(0, 0);
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

