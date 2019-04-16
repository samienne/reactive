#pragma once

#include "avgvisibility.h"

#include <ase/buffer.h>
#include <ase/uniformset.h>
#include <ase/uniformbuffer.h>
#include <ase/pipeline.h>
#include <ase/rendercontext.h>
#include <ase/vector.h>

namespace avg
{
    class Pen;
    class Brush;

    class AVG_EXPORT Painter final
    {
    public:
        Painter(ase::RenderContext& context);
        ~Painter();

        void setSize(ase::Vector2i size);

        ase::UniformSet const& getUniformSet() const;

        Painter(Painter const&) = default;
        Painter(Painter&&) = default;

        Painter& operator=(Painter const&) = default;
        Painter& operator=(Painter&&) = default;

        ase::Pipeline const& getPipeline(Brush const& brush) const;
        ase::Pipeline const& getPipeline(Pen const& pen) const;

    private:
        ase::Pipeline solidPipeline_;
        ase::Pipeline transparentPipeline_;
        ase::Buffer buffer_;
        ase::UniformSet uniformSet_;
        ase::UniformBuffer uniformBuffer_;
    };
}

