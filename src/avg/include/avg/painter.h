#pragma once

#include "drawing.h"
#include "passpipelinecache.h"
#include "scratchpool.h"
#include "targetimage.h"

#include "avgvisibility.h"

#include <ase/commandbuffer.h>
#include <ase/indexbuffer.h>
#include <ase/matrix.h>
#include <ase/uniformblock.h>
#include <ase/uniformring.h>
#include <ase/uniformset.h>
#include <ase/pipeline.h>
#include <ase/rendercontext.h>
#include <ase/vector.h>
#include <ase/vertexbuffer.h>

#include <pmr/memory_resource.h>

#include <memory>
#include <optional>

namespace avg
{
    class Pen;
    class Brush;
    class GlyphAtlas;
    class EffectEmitter;

    /**
     * @brief Records drawing into frames and submits them.
     *
     * A frame is everything recorded from the first clear or paint up to
     * flush.
     *
     * A painter serves one window: window frames are drawn into an
     * offscreen target of the window's pixel size that `presentWindow`
     * copies onto the window, so painting two windows through one painter
     * would re-create that target every frame.
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
        void paintToImage(TargetImage& target, float scalingFactor, Drawing const& drawing);

        /**
         * @brief Clear the window's offscreen target: colour to opaque
         * black and depth.
         *
         * The window itself is never cleared; `presentWindow` covers it.
         */
        void clearWindow(ase::Window& target);

        /**
         * @brief Draw into the window's offscreen target.
         */
        void paintToWindow(ase::Window& target, Drawing const& drawing);

        /**
         * @brief Copy the offscreen target onto the window with a
         * full-window quad, flush, and present the window.
         *
         * Nothing is copied before the first `clearWindow` or
         * `paintToWindow`; after that a frame that painted nothing presents
         * the target as it was.
         */
        void presentWindow(ase::Window& target);

        void flush();

        /**
         * @brief The emitter that records this frame's backdrop effects
         * into commands, over mainTarget.
         *
         * Opens the frame if none is open.
         */
        EffectEmitter makeEffectEmitter(ase::CommandBuffer& commands,
                TargetImage& mainTarget);

    private:
        struct MatrixBlock
        {
            ase::Matrix4f worldViewProj;
        };

        void beginFrame();
        void writeMatrices();
        TargetImage& windowTarget(ase::Window& window);
        void pushPresent(ase::Window& window);

        pmr::memory_resource* memory_;
        ase::RenderContext& renderContext_;
        ase::Pipeline solidPipeline_;
        ase::Pipeline transparentPipeline_;
        ase::Pipeline textPipeline_;
        ase::Pipeline presentPipeline_;
        ase::UniformSet presentUniformSet_;
        std::optional<TargetImage> windowTarget_;
        std::optional<ase::VertexBuffer> presentVertices_;
        std::optional<ase::IndexBuffer> presentIndices_;
        std::unique_ptr<GlyphAtlas> glyphAtlas_;
        ase::UniformBlock<MatrixBlock> matrices_;
        ase::UniformRing uniformRing_;
        ase::UniformSet uniformSet_;
        ScratchPool scratchPool_;
        PassPipelineCache passPipelines_;
        ase::CommandBuffer commandBuffer_;
        bool frameOpen_ = false;
        bool scratchFrameOpen_ = false;
    };
}

