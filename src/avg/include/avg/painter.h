#pragma once

#include "drawing.h"
#include "targetimage.h"

#include "avgvisibility.h"

#include <ase/commandbuffer.h>
#include <ase/matrix.h>
#include <ase/uniformblock.h>
#include <ase/uniformring.h>
#include <ase/uniformset.h>
#include <ase/pipeline.h>
#include <ase/rendercontext.h>
#include <ase/vector.h>

#include <pmr/memory_resource.h>

#include <memory>

namespace avg
{
    class Pen;
    class Brush;
    class GlyphAtlas;

    /**
     * @brief Records drawing into frames and submits them.
     *
     * A frame is everything recorded from the first clear or paint up to
     * flush.
     */
    class AVG_EXPORT Painter final
    {
    public:
        Painter(pmr::memory_resource* memory, ase::RenderContext& context);
        ~Painter();

        Painter(Painter const&) = delete;
        Painter(Painter&&) noexcept;

        pmr::memory_resource* getResource() const;

        Painter& operator=(Painter const&) = delete;
        Painter& operator=(Painter&&) = delete;

        /**
         * @brief Fit the projection to a framebuffer.
         *
         * Vertices are device pixels over the scaling factor, so one logical
         * unit maps to scalingFactor pixels of a pixelSize framebuffer,
         * whatever the rounded logical size is.
         */
        void setSize(ase::Vector2i pixelSize, float scalingFactor);

        ase::UniformSet const& getUniformSet() const;

        ase::Pipeline const& getPipeline(Brush const& brush) const;
        ase::Pipeline const& getPipeline(Pen const& pen) const;

        /**
         * @brief The pipeline that draws glyph quads sampling the atlas.
         *
         * Its vertices are rgba, xyz, uv; the atlas texture binds at unit 0.
         */
        ase::Pipeline const& getTextPipeline() const;

        /**
         * @brief This painter's glyph atlas: one per painter, so per window.
         *
         * GlyphAtlas is a private header; this serves avg's renderer and
         * tests.
         */
        GlyphAtlas& getGlyphAtlas();

        void clearImage(TargetImage& target);
        void clearWindow(ase::Window& target);
        void paintToImage(TargetImage& target, float scalingFactor, Drawing const& drawing);
        void paintToWindow(ase::Window& target, Drawing const& drawing);
        void presentWindow(ase::Window& target);
        void flush();

    private:
        struct MatrixBlock
        {
            ase::Matrix4f worldViewProj;
        };

        void beginFrame();
        void writeMatrices();

        pmr::memory_resource* memory_;
        ase::RenderContext& renderContext_;
        ase::Pipeline solidPipeline_;
        ase::Pipeline transparentPipeline_;
        ase::Pipeline textPipeline_;
        std::unique_ptr<GlyphAtlas> glyphAtlas_;
        ase::UniformBlock<MatrixBlock> matrices_;
        ase::UniformRing uniformRing_;
        ase::UniformSet uniformSet_;
        ase::CommandBuffer commandBuffer_;
        bool frameOpen_ = false;
    };
}

