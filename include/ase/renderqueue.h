#pragma once

#include "rendercommand.h"

#include <btl/visibility.h>

#include <vector>

namespace ase
{
    class RenderCommand;

    class BTL_VISIBLE RenderQueue
    {
    public:
        using Iterator = std::vector<RenderCommand>::iterator;
        using ConstIterator = std::vector<RenderCommand>::const_iterator;

        void push(
                RenderTarget target,
                Pipeline pipeline,
                UniformBuffer uniforms,
                VertexBuffer vertexBuffer,
                IndexBuffer indexBuffer,
                std::vector<Texture> textures,
                float z);

        Iterator begin();
        Iterator end();

        ConstIterator begin() const;
        ConstIterator end() const;

    private:
        std::vector<RenderCommand> commands_;
    };
} // namespace ase

