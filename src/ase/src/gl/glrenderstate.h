#pragma once

#include "framebufferobjectcache.h"
#include "vector.h"
#include "scissorrect.h"

#include "asevisibility.h"

#include "systemgl.h"

#include <btl/future/promise.h>

#include <chrono>
#include <optional>

namespace ase
{
    class GlRenderContext;
    class CommandBuffer;
    class VertexSpec;
    class UniformBuffer;
    class FramebufferImpl;
    class GlDispatchedContext;
    class GlUniformSet;
    struct GlFunctions;
    struct Dispatched;

    class ASE_EXPORT GlRenderState
    {
    public:
        GlRenderState(GlDispatchedContext& dispatcher);
        ~GlRenderState();

        void submit(CommandBuffer&& commands);
        void clear(Dispatched, GLbitfield mask);
        void setViewport(Dispatched, Vector2i size);

        /**
         * @brief Binds a framebuffer object as the draw and read target.
         *
         * Name 0 is the current drawable's own framebuffer.
         */
        void bindFramebuffer(Dispatched, GlFunctions const& gl,
                GLuint framebuffer);
        void setScissor(Dispatched, std::optional<ScissorRect> const& scissor);

    private:
        struct GlFramebufferObject
        {
            GLuint name;
            Vector2i size;
        };

        // These functions need to be called in dispatched context.
        void pushSpec(Dispatched, GlFunctions const& gl, VertexSpec const& spec,
                std::vector<int>& activeAttribs);
        void dispatchedRenderQueue(Dispatched, GlFunctions const& gl,
                CommandBuffer&& commands);
        void checkFences(Dispatched, GlFunctions const& gl);
        void bindTarget(Dispatched, GlFunctions const& gl,
                FramebufferImpl const& framebuffer);
        GlFramebufferObject createFramebufferObject(Dispatched,
                GlFunctions const& gl,
                AttachmentFramebuffer::Attachments const& attachments);
        void deleteFramebufferObject(Dispatched, GlFunctions const& gl,
                GlFramebufferObject const& object);
        void sweepFramebufferObjects(Dispatched, GlFunctions const& gl);
        void onIdle(Dispatched, GlFunctions const& gl);
        void updateIdle(Dispatched);

    private:
        GlDispatchedContext& dispatcher_;

        // Active sync objects
        struct WaitingFence
        {
            GLsync sync;
            btl::future::Promise<> promise;
        };

        std::vector<WaitingFence> fences_;

        FramebufferObjectCache<GlFramebufferObject> framebufferObjects_;
        std::optional<std::chrono::milliseconds> idlePeriod_;

        // Current state
        Vector2i viewportSize_ = Vector2i(0, 0);
        std::optional<ScissorRect> scissor_;
        FramebufferImpl const* boundFramebuffer_ = nullptr;
        uint64_t boundFramebufferVersion_ = 0;
        std::optional<GLuint> boundFramebufferObject_;
        GLuint vertexArrayObject_ = 0;
        GLuint boundProgram_ = 0;
        GLuint boundVbo_ = 0;
        GLuint boundIbo_ = 0;
        GLenum srcFactor_ = 0;
        GLenum dstFactor_ = 0;
        GlUniformSet const* boundUniformSet_ = nullptr;
        uint64_t boundUniformSetVersion_ = 0;
        std::vector<GLint> activeAttribs_;
        std::vector<GLuint> activeTextures_;
        bool depthTest_ = true;
        bool depthWrite_ = true;
        bool blendEnabled_ = false;
    };
} // namespace ase

