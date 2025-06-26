#include "commandbuffer.h"

#include "rendercommand.h"
#include "textureimpl.h"

#include <iostream>

namespace ase
{

void CommandBuffer::push(
        Framebuffer framebuffer,
        Pipeline pipeline,
        UniformSet uniforms,
        VertexBuffer vertexBuffer,
        std::optional<IndexBuffer> indexBuffer,
        std::vector<Texture> textures,
        float z)
{
    commands_.push_back(DrawCommand(
                std::move(framebuffer),
                std::move(pipeline),
                std::move(uniforms),
                std::move(vertexBuffer),
                std::move(indexBuffer),
                std::move(textures),
                z
                ));
}

void CommandBuffer::pushClear(Framebuffer target,
        float r,
        float g,
        float b,
        float a,
        bool color,
        bool depth,
        bool stencil
        )
{
    commands_.push_back(ClearCommand{
            std::move(target),
            r, g, b, a, color, depth, stencil
            });
}

void CommandBuffer::pushPresent(Window window)
{
    commands_.push_back(PresentCommand{
            std::move(window)
            });
}

void CommandBuffer::pushFence(std::function<void()> completeCb)
{
    pushFence().then(std::move(completeCb)).detach();
}

btl::future::Future<> CommandBuffer::pushFence()
{
    auto control = std::make_shared<btl::future::FutureControl<>>();

    commands_.push_back(FenceCommand{ { control } });

    return { std::move(control) };
}

void CommandBuffer::pushUpload(
        std::variant<VertexBuffer, IndexBuffer, UniformBuffer> target,
        Buffer data,
        Usage usage
        )
{
    commands_.push_back(BufferUploadCommand
            {
                std::move(target),
                std::move(data),
                usage
            });
}

void CommandBuffer::pushUpload(Texture target, Buffer data, Vector2i size, Format format)
{
    target.getImpl<TextureImpl>().setSize(size);
    target.getImpl<TextureImpl>().setFormat(format);

    commands_.push_back(TextureUploadCommand
            {
                std::move(target),
                std::move(data),
                size,
                format
            });
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


