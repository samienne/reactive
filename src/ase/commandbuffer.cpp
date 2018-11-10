#include "commandbuffer.h"

#include "rendercommand.h"

namespace ase
{

void CommandBuffer::push(
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

CommandBuffer::Iterator CommandBuffer::begin()
{
    return commands_.begin();
}

CommandBuffer::Iterator CommandBuffer::end()
{
    return commands_.end();
}

CommandBuffer::ConstIterator CommandBuffer::begin() const
{
    return commands_.begin();
}

CommandBuffer::ConstIterator CommandBuffer::end() const
{
    return commands_.end();
}

} // namespace ase


