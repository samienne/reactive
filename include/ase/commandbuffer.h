#pragma once

#include "rendercommand.h"

#include <btl/option.h>
#include <btl/visibility.h>

#include <vector>

namespace ase
{
    class RenderCommand;

    class BTL_VISIBLE CommandBuffer
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

        size_t size() const;
        Iterator begin();
        Iterator end();

        ConstIterator begin() const;
        ConstIterator end() const;

    private:
        std::vector<RenderCommand> commands_;
    };
} // namespace ase


