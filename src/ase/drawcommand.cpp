#include "drawcommand.h"

namespace ase
{

DrawCommandDeferred::DrawCommandDeferred(
        Framebuffer framebuffer,
        std::vector<Texture> textures,
        Pipeline pipeline,
        VertexBuffer vertexBuffer,
        std::optional<IndexBuffer> indexBuffer,
        UniformSet uniforms,
        float z) :
    framebuffer_(std::move(framebuffer)),
    textures_(std::move(textures)),
    pipeline_(std::move(pipeline)),
    vertexBuffer_(std::move(vertexBuffer)),
    indexBuffer_(std::move(indexBuffer)),
    uniforms_(std::move(uniforms)),
    z_(z)
{
}

DrawCommand::DrawCommand(
        Framebuffer framebuffer,
        Pipeline pipeline,
        UniformSet uniforms,
        VertexBuffer vertexBuffer,
        std::optional<IndexBuffer> indexBuffer,
        std::vector<Texture> textures,
        float z)
    : deferred_(
            std::move(framebuffer),
            std::move(textures),
            std::move(pipeline),
            std::move(vertexBuffer),
            std::move(indexBuffer),
            std::move(uniforms),
            z)
{
}

DrawCommand::~DrawCommand()
{
}

} // namespace

