#include "rendercommand.h"

#include "rendertarget.h"

namespace ase
{

RenderCommandDeferred::RenderCommandDeferred(
        RenderTarget renderTarget,
        std::vector<Texture> textures,
        Pipeline pipeline,
        VertexBuffer vertexBuffer,
        btl::option<IndexBuffer> indexBuffer,
        UniformBuffer uniforms,
        float z) :
    renderTarget_(std::move(renderTarget)),
    textures_(std::move(textures)),
    pipeline_(std::move(pipeline)),
    vertexBuffer_(std::move(vertexBuffer)),
    indexBuffer_(std::move(indexBuffer)),
    uniforms_(std::move(uniforms)),
    z_(z)
{
}

RenderCommand::RenderCommand(
        RenderTarget renderTarget,
        Pipeline pipeline,
        UniformBuffer uniforms,
        VertexBuffer vertexBuffer,
        btl::option<IndexBuffer> indexBuffer,
        std::vector<Texture> textures,
        float z)
    : deferred_(
            std::move(renderTarget),
            std::move(textures),
            std::move(pipeline),
            std::move(vertexBuffer),
            std::move(indexBuffer),
            std::move(uniforms),
            z)
{
}

RenderCommand::~RenderCommand()
{
}

} // namespace

