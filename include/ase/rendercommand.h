#pragma once

#include "blendmode.h"
#include "texture.h"
#include "pipeline.h"
#include "uniformbuffer.h"
#include "vertexbuffer.h"
#include "indexbuffer.h"
#include "vertexspec.h"
#include "framebuffer.h"
#include "uniformset.h"

#include <btl/option.h>
#include <btl/visibility.h>

#include <vector>
#include <memory>

namespace ase
{
    struct BTL_VISIBLE RenderCommandDeferred
    {
        RenderCommandDeferred(
                Framebuffer framebuffer,
                std::vector<Texture> textures,
                Pipeline pipeline,
                VertexBuffer vertexBuffer,
                btl::option<IndexBuffer> indexBuffer,
                UniformSet uniforms,
                float z);

        RenderCommandDeferred(RenderCommandDeferred const&) = default;
        RenderCommandDeferred(RenderCommandDeferred&&) noexcept = default;
        RenderCommandDeferred& operator=(
                RenderCommandDeferred const&) = default;
        RenderCommandDeferred& operator=(RenderCommandDeferred&&) noexcept = default;

        Framebuffer framebuffer_;

        // Textures
        std::vector<Texture> textures_;

        Pipeline pipeline_;

        // Vertex data
        VertexBuffer vertexBuffer_;
        btl::option<IndexBuffer> indexBuffer_;

        // Uniforms
        UniformSet uniforms_;

        float z_;
    };

    class BTL_VISIBLE RenderCommand
    {
    public:
        RenderCommand(
                Framebuffer framebuffer,
                Pipeline pipeline,
                UniformSet uniforms,
                VertexBuffer vertexBuffer,
                btl::option<IndexBuffer> indexBuffer,
                std::vector<Texture> textures,
                float z);

        RenderCommand(RenderCommand const&) = default;
        RenderCommand(RenderCommand&&) = default;
        ~RenderCommand();

        RenderCommand& operator=(RenderCommand const&) = default;
        RenderCommand& operator=(RenderCommand&&) = default;

        inline Framebuffer const& getFramebuffer() const
        {
            return d()->framebuffer_;
        }

        inline std::vector<Texture> const& getTextures() const
        {
            return d()->textures_;
        }

        inline Pipeline const& getPipeline() const
        {
            return d()->pipeline_;
        }

        inline VertexBuffer const& getVertexBuffer() const
        {
            return d()->vertexBuffer_;
        }

        inline btl::option<IndexBuffer> const& getIndexBuffer() const
        {
            return d()->indexBuffer_;
        }

        inline UniformSet const& getUniforms() const
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

