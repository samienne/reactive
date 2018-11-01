#include "glpipeline.h"

#include "namedvertexspec.h"
#include "vertexspec.h"

#include "debug.h"

namespace ase
{

GlPipeline::GlPipeline(
        Program program,
        VertexSpec vertexSpec
        ) :
    program_(std::move(program)),
    vertexSpec_(vertexSpec),
    srcFactor_(BlendMode::Zero),
    dstFactor_(BlendMode::Zero),
    blendEnabled_(false)
{
}

GlPipeline::GlPipeline(
        Program program,
        VertexSpec vertexSpec,
        BlendMode srcFactor,
        BlendMode dstFactor
        ) :
    program_(std::move(program)),
    vertexSpec_(vertexSpec),
    srcFactor_(srcFactor),
    dstFactor_(dstFactor),
    blendEnabled_(true)
{
}

bool GlPipeline::operator==(GlPipeline const& rhs) const
{
    return program_ == rhs.program_
        && vertexSpec_ == rhs.vertexSpec_
        && blendEnabled_ == rhs.blendEnabled_
        && srcFactor_ == rhs.srcFactor_
        && dstFactor_ == rhs.dstFactor_;
}

bool GlPipeline::operator!=(GlPipeline const& rhs) const
{
    return program_ != rhs.program_
        || vertexSpec_ != rhs.vertexSpec_
        || blendEnabled_ != rhs.blendEnabled_
        || srcFactor_ != rhs.srcFactor_
        || dstFactor_ != rhs.dstFactor_;
}

bool GlPipeline::operator<(GlPipeline const& rhs) const
{
    if (blendEnabled_ != rhs.blendEnabled_)
        return !blendEnabled_;

    if (srcFactor_ != rhs.srcFactor_)
        return srcFactor_ < rhs.srcFactor_;

    if (dstFactor_ != rhs.dstFactor_)
        return dstFactor_ < rhs.dstFactor_;

    if (program_ != rhs.program_)
        return program_ < rhs.program_;

    return vertexSpec_ < rhs.vertexSpec_;
}

Program const& GlPipeline::getProgram() const
{
    return program_;
}

VertexSpec const& GlPipeline::getVertexSpec() const
{
    return vertexSpec_;
}

bool GlPipeline::isBlendEnabled() const
{
    return blendEnabled_;
}

BlendMode GlPipeline::getSrcFactor() const
{
    return srcFactor_;
}

BlendMode GlPipeline::getDstFactor() const
{
    return dstFactor_;
}

} // namespace
