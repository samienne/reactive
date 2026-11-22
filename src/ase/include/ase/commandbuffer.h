#pragma once

#include "rendercommand.h"
#include "readback.h"
#include "asevisibility.h"

#include <btl/future.h>

#include <vector>
#include <optional>

namespace ase
{
    class ASE_EXPORT CommandBuffer
    {
    public:
        using Iterator = std::vector<RenderCommand>::iterator;
        using ConstIterator = std::vector<RenderCommand>::const_iterator;

        void push(
                Framebuffer framebuffer,
                Pipeline pipeline,
                UniformSet uniforms,
                VertexBuffer vertexBuffer,
                std::optional<IndexBuffer> indexBuffer,
                std::vector<Texture> textures,
                float z);

        void pushClear(Framebuffer target,
                float r = 0.0f,
                float g = 0.0f,
                float b = 0.0f,
                float a = 1.0f,
                bool color = true,
                bool depth = true,
                bool stencil = false
                );

        void pushFence(std::function<void()> completeCb);
        btl::future::Future<> pushFence();

        void pushUpload(
                std::variant<VertexBuffer, IndexBuffer, UniformBuffer> target,
                Buffer data,
                Usage usage
                );

        void pushUpload(Texture target, Buffer data, Vector2i size, Format format);

        /**
         * @brief Read back the whole colour of `source` once the commands
         * pushed before it have drawn.
         *
         * The size is the framebuffer's when the command runs, and the image
         * carries it. The future completes on any thread with the pixels, or
         * fails with `ReadbackFailure`: `ReadbackError::Unsupported` on a
         * render context whose `canReadback()` is false, or
         * `ReadbackError::Failed` when the backend cannot read. It fails with
         * `btl::future::BrokenPromise` if the command is dropped unrun, as
         * when the render context is destroyed first.
         */
        btl::future::Future<ReadbackImage> pushReadback(Framebuffer source);

        size_t size() const;
        Iterator begin();
        Iterator end();

        ConstIterator begin() const;
        ConstIterator end() const;

    private:
        std::vector<RenderCommand> commands_;
    };
} // namespace ase


