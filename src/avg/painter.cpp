#include "painter.h"
#include "brush.h"
#include "pen.h"

#include <ase/namedvertexspec.h>
#include <ase/vertexshader.h>
#include <ase/fragmentshader.h>
#include <ase/program.h>
#include <ase/blendmode.h>

static char const* simpleVsSource =
"#version 330\n"
"#extension GL_ARB_shading_language_420pack : require\n"
"\n"
"layout(std140, binding = 0) uniform MatrixBlock\n"
"{\n"
"   mat4 worldViewProj;\n"
"} matrices;\n"
"\n"
"attribute vec4 color;\n"
"attribute vec3 pos;\n"
"\n"
"varying vec4 vColor;\n"
"\n"
"void main()\n"
"{\n"
"   vColor = color;\n"
"   gl_Position = matrices.worldViewProj * vec4(pos, 1.0);\n"
"}\n";

static char const* simpleFsSource =
"varying vec4 vColor;\n"
"\n"
"void main()\n"
"{\n"
"   gl_FragColor = vColor;\n"
"}\n";

namespace avg
{

namespace
{
    ase::Pipeline makePipeline(ase::RenderContext& context, bool solid)
    {
        ase::NamedVertexSpec namedSpec;
        namedSpec.add("color", 4, ase::TypeFloat, false)
            .add("pos", 3, ase::TypeFloat, false);
        auto vs = context.makeVertexShader(simpleVsSource);
        auto fs = context.makeFragmentShader(simpleFsSource);
        auto program = context.makeProgram(std::move(vs), std::move(fs));
        ase::VertexSpec spec(program, std::move(namedSpec));

        if (solid)
            return context.makePipeline(program, spec);
        else
        {
            return context.makePipelineWithBlend(program, spec,
                    ase::BlendMode::One, ase::BlendMode::OneMinusSrcAlpha);
        }
    }
} // anonymous namespace

Painter::Painter(ase::RenderContext& context) :
    solidPipeline_(makePipeline(context, true)),
    transparentPipeline_(makePipeline(context, false))
{
}

Painter::~Painter()
{
}

ase::Pipeline const& Painter::getPipeline(Brush const& brush) const
{
    if (brush.getColor().getAlpha() > 254.0f/255.0f)
        return solidPipeline_;
    else
        return transparentPipeline_;
}

ase::Pipeline const& Painter::getPipeline(Pen const& pen) const
{
    if (std::abs(pen.getBrush().getColor().getAlpha()) - 1.0f > 0.0001f)
        return solidPipeline_;
    else
        return transparentPipeline_;
}

} // namespace
