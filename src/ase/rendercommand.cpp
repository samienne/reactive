#include "rendercommand.h"

#include "rendertarget.h"

namespace ase
{

RenderCommandDeferred::RenderCommandDeferred(
        std::vector<Texture> textures,
        Pipeline pipeline,
        VertexBuffer vertexBuffer,
        IndexBuffer indexBuffer,
        UniformBuffer uniforms,
        float z) :
    textures_(std::move(textures)),
    pipeline_(std::move(pipeline)),
    vertexBuffer_(std::move(vertexBuffer)),
    indexBuffer_(std::move(indexBuffer)),
    uniforms_(std::move(uniforms)),
    z_(z)
{
}

RenderCommand::RenderCommand(Pipeline const& pipeline,
        UniformBuffer const& uniforms, VertexBuffer const& vertexBuffer,
        IndexBuffer const& indexBuffer, std::vector<Texture> const& textures,
        float z)
    : deferred_(textures, pipeline, vertexBuffer, indexBuffer, uniforms, z)
{
}

RenderCommand::~RenderCommand()
{
}

} // namespace

