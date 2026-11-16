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
         * @brief Read back the colour of `source` once the commands pushed
         * before it have drawn.
         *
         * Reads the `size` pixels whose top-left corner is `offset`, both in
         * pixels from the framebuffer's top-left corner, rows running down as
         * in `ReadbackImage`: `pushReadback(fb, { 0, 0 }, { 64, 32 }, cb)`
         * reads the top-left 64x32 pixels. `callback` receives the pixels;
         * `ReadbackError::Unsupported` on a render context whose
         * `canReadback()` is false; or `ReadbackError::Failed` when the
         * region does not fit the framebuffer. It runs once, on any thread.
         */
        void pushReadback(Framebuffer source, Vector2i offset, Vector2i size,
                ReadbackCallback callback);

        size_t size() const;
        Iterator begin();
        Iterator end();

        ConstIterator begin() const;
        ConstIterator end() const;

    private:
        std::vector<RenderCommand> commands_;
    };
} // namespace ase


