#include "renderqueue.h"

#include "rendercommand.h"

namespace ase
{

void RenderQueue::push(
        RenderTarget target,
        Pipeline pipeline,
        UniformBuffer uniforms,
        VertexBuffer vertexBuffer,
        IndexBuffer indexBuffer,
        std::vector<Texture> textures,
        float z)
{
    commands_.emplace_back(
            std::move(target),
            std::move(pipeline),
            std::move(uniforms),
            std::move(vertexBuffer),
            std::move(indexBuffer),
            std::move(textures),
            z);
}

} // namespace ase

