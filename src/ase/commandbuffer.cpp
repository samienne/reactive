#include "commandbuffer.h"

#include "rendercommand.h"

#include <iostream>

namespace ase
{

void CommandBuffer::push(
        Framebuffer framebuffer,
        Pipeline pipeline,
        UniformBuffer uniforms,
        VertexBuffer vertexBuffer,
        btl::option<IndexBuffer> indexBuffer,
        std::vector<Texture> textures,
        float z)
{
    commands_.emplace_back(
            std::move(framebuffer),
            std::move(pipeline),
            std::move(uniforms),
            std::move(vertexBuffer),
            std::move(indexBuffer),
            std::move(textures),
            z);
}

size_t CommandBuffer::size() const
{
    return commands_.size();
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


