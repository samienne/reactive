#pragma once

#include "drawing.h"
#include "targetimage.h"

#include "avgvisibility.h"

#include <ase/commandbuffer.h>
#include <ase/buffer.h>
#include <ase/uniformset.h>
#include <ase/uniformbuffer.h>
#include <ase/pipeline.h>
#include <ase/rendercontext.h>
#include <ase/vector.h>

#include <pmr/memory_resource.h>

namespace avg
{
    class Pen;
    class Brush;

    class AVG_EXPORT Painter final
    {
    public:
        Painter(pmr::memory_resource* memory, ase::RenderContext& context);
        ~Painter();

        Painter(Painter const&) = default;
        Painter(Painter&&) = default;

        //Painter& operator=(Painter const&) = default;
        //Painter& operator=(Painter&&) = default;

        void setSize(ase::Vector2i size);

        ase::UniformSet const& getUniformSet() const;

        ase::Pipeline const& getPipeline(Brush const& brush) const;
        ase::Pipeline const& getPipeline(Pen const& pen) const;

        void renderToImage(TargetImage& target, float scalingFactor,
                Drawing const& drawing);
        void renderToFramebuffer(ase::Window& target, Drawing const& drawing);
        void flushContext() const;

    private:
        pmr::memory_resource* memory_;
        ase::RenderContext& renderContext_;
        ase::Pipeline solidPipeline_;
        ase::Pipeline transparentPipeline_;
        ase::Buffer buffer_;
        ase::UniformSet uniformSet_;
        ase::UniformBuffer uniformBuffer_;
        ase::CommandBuffer commandBuffer_;
    };
}

