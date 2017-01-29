#pragma once

#include <ase/pipeline.h>
#include <ase/rendercontext.h>

namespace avg
{
    class Pen;
    class Brush;

    class Painter final
    {
    public:
        Painter(ase::RenderContext& context);
        ~Painter();

        Painter(Painter const&) = default;
        Painter(Painter&&) = default;

        Painter& operator=(Painter const&) = default;
        Painter& operator=(Painter&&) = default;

        ase::Pipeline const& getPipeline(Brush const& brush) const;
        ase::Pipeline const& getPipeline(Pen const& pen) const;

    private:
        ase::Pipeline solidPipeline_;
        ase::Pipeline transparentPipeline_;
    };
}

