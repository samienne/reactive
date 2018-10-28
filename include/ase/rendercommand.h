#pragma once

#include "blendmode.h"
#include "texture.h"
#include "pipeline.h"
#include "uniformbuffer.h"
#include "vertexbuffer.h"
#include "indexbuffer.h"
#include "vertexspec.h"
#include "rendertarget.h"

#include <btl/visibility.h>

#include <vector>
#include <memory>

namespace ase
{
    struct BTL_VISIBLE RenderCommandDeferred
    {
        RenderCommandDeferred(
                RenderTarget target,
                std::vector<Texture> textures,
                Pipeline pipeline,
                VertexBuffer vertexBuffer,
                IndexBuffer indexBuffer,
                UniformBuffer uniforms,
                float z);

        RenderCommandDeferred(RenderCommandDeferred const&) = default;
        RenderCommandDeferred(RenderCommandDeferred&&) noexcept = default;
        RenderCommandDeferred& operator=(
                RenderCommandDeferred const&) = default;
        RenderCommandDeferred& operator=(RenderCommandDeferred&&) noexcept = default;

        RenderTarget renderTarget_;

        // Textures
        std::vector<Texture> textures_;

        Pipeline pipeline_;

        // Vertex data
        VertexBuffer vertexBuffer_;
        IndexBuffer indexBuffer_;

        // Uniforms
        UniformBuffer uniforms_;

        float z_;
    };

    class BTL_VISIBLE RenderCommand
    {
    public:
        RenderCommand(
                RenderTarget target,
                Pipeline pipeline,
                UniformBuffer uniforms,
                VertexBuffer vertexBuffer,
                IndexBuffer indexBuffer,
                std::vector<Texture> textures,
                float z);

        RenderCommand(RenderCommand const&) = default;
        RenderCommand(RenderCommand&&) = default;
        ~RenderCommand();

        RenderCommand& operator=(RenderCommand const&) = default;
        RenderCommand& operator=(RenderCommand&&) = default;

        inline RenderTarget const& getRenderTarget() const
        {
            return d()->renderTarget_;
        }

        inline std::vector<Texture> const& getTextures() const
        {
            return d()->textures_;
        }

        inline Pipeline const& getPipeline() const
        {
            return d()->pipeline_;
        }

        inline Program const& getProgram() const
        {
            return d()->pipeline_.getProgram();
        }

        inline VertexBuffer const& getVertexBuffer() const
        {
            return d()->vertexBuffer_;
        }

        inline VertexSpec const& getVertexSpec() const
        {
            return d()->pipeline_.getSpec();
        }

        inline IndexBuffer const& getIndexBuffer() const
        {
            return d()->indexBuffer_;
        }

        inline UniformBuffer const& getUniforms() const
        {
            return d()->uniforms_;
        }

        inline float getZ() const
        {
            return d()->z_;
        }

    private:
        RenderCommandDeferred deferred_;
        inline RenderCommandDeferred* d() { return &deferred_; }
        inline RenderCommandDeferred const* d() const
        {
            return &deferred_;
        }
    };
}

