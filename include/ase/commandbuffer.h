#pragma once

#include "rendercommand.h"

#include <btl/option.h>
#include "asevisibility.h"

#include <vector>

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
                btl::option<IndexBuffer> indexBuffer,
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

        void pushPresent(Window window);

        void pushFence(std::function<void()> completeCb);

        void pushUpload(
                std::variant<VertexBuffer, IndexBuffer, UniformBuffer> target,
                Buffer data,
                Usage usage
                );

        void pushUpload(Texture target, Buffer data, Vector2i size, Format format);

        size_t size() const;
        Iterator begin();
        Iterator end();

        ConstIterator begin() const;
        ConstIterator end() const;

    private:
        std::vector<RenderCommand> commands_;
    };
} // namespace ase


