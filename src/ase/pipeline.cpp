#include "pipeline.h"

#include "namedvertexspec.h"
#include "vertexspec.h"

#include "debug.h"

namespace ase
{

Pipeline::Pipeline(RenderContext& /*context*/, Program program,
            VertexSpec vertexSpec) :
    program_(std::move(program)),
    vertexSpec_(vertexSpec),
    srcFactor_(BlendMode::Zero),
    dstFactor_(BlendMode::Zero),
    blendEnabled_(false)
{
}

Pipeline::Pipeline(RenderContext& /*context*/, Program program,
            VertexSpec vertexSpec, BlendMode srcFactor, BlendMode dstFactor) :
    program_(std::move(program)),
    vertexSpec_(vertexSpec),
    srcFactor_(srcFactor),
    dstFactor_(dstFactor),
    blendEnabled_(true)
{
}

bool Pipeline::operator==(Pipeline const& rhs) const
{
    return program_ == rhs.program_
        && vertexSpec_ == rhs.vertexSpec_
        && blendEnabled_ == rhs.blendEnabled_
        && srcFactor_ == rhs.srcFactor_
        && dstFactor_ == rhs.dstFactor_;
}

bool Pipeline::operator!=(Pipeline const& rhs) const
{
    return program_ != rhs.program_
        || vertexSpec_ != rhs.vertexSpec_
        || blendEnabled_ != rhs.blendEnabled_
        || srcFactor_ != rhs.srcFactor_
        || dstFactor_ != rhs.dstFactor_;
}

bool Pipeline::operator<(Pipeline const& rhs) const
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

Program const& Pipeline::getProgram() const
{
    return program_;
}

VertexSpec const& Pipeline::getSpec() const
{
    return vertexSpec_;
}

bool Pipeline::isBlendEnabled() const
{
    return blendEnabled_;
}

BlendMode Pipeline::getSrcFactor() const
{
    return srcFactor_;
}

BlendMode Pipeline::getDstFactor() const
{
    return dstFactor_;
}

} // namespace

