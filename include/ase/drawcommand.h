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
#include "asevisibility.h"

#include <vector>
#include <memory>

namespace ase
{
    struct ASE_EXPORT DrawCommandDeferred
    {
        DrawCommandDeferred(
                Framebuffer framebuffer,
                std::vector<Texture> textures,
                Pipeline pipeline,
                VertexBuffer vertexBuffer,
                btl::option<IndexBuffer> indexBuffer,
                UniformSet uniforms,
                float z);

        DrawCommandDeferred(DrawCommandDeferred const&) = default;
        DrawCommandDeferred(DrawCommandDeferred&&) noexcept = default;
        DrawCommandDeferred& operator=(
                DrawCommandDeferred const&) = default;
        DrawCommandDeferred& operator=(DrawCommandDeferred&&) noexcept = default;

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

    class ASE_EXPORT DrawCommand
    {
    public:
        DrawCommand(
                Framebuffer framebuffer,
                Pipeline pipeline,
                UniformSet uniforms,
                VertexBuffer vertexBuffer,
                btl::option<IndexBuffer> indexBuffer,
                std::vector<Texture> textures,
                float z);

        DrawCommand(DrawCommand const&) = default;
        DrawCommand(DrawCommand&&) = default;
        ~DrawCommand();

        DrawCommand& operator=(DrawCommand const&) = default;
        DrawCommand& operator=(DrawCommand&&) = default;

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
        DrawCommandDeferred deferred_;
        inline DrawCommandDeferred* d() { return &deferred_; }
        inline DrawCommandDeferred const* d() const
        {
            return &deferred_;
        }
    };
}


