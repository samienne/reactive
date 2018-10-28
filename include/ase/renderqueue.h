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
        void push(
                RenderTarget target,
                Pipeline pipeline,
                UniformBuffer uniforms,
                VertexBuffer vertexBuffer,
                IndexBuffer indexBuffer,
                std::vector<Texture> textures,
                float z);

    private:
        friend class RenderContext;

        std::vector<RenderCommand> commands_;
    };
} // namespace ase

