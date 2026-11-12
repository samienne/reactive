#include "glpipeline.h"

#include "glrendercontext.h"
#include "gltype.h"

#include "namedvertexspec.h"
#include "vertexspec.h"

#include "debug.h"

namespace ase
{

GlPipeline::GlPipeline(
        GlRenderContext& /*context*/,
        Program program,
        VertexSpec vertexSpec,
        PipelineDesc desc
        ) :
    program_(std::move(program)),
    vertexSpec_(vertexSpec),
    colorFormat_(desc.colorFormat),
    depthFormat_(desc.depthFormat),
    srcFactor_(desc.blend ? desc.blend->srcFactor : BlendMode::Zero),
    dstFactor_(desc.blend ? desc.blend->dstFactor : BlendMode::Zero),
    blendEnabled_(desc.blend.has_value()),
    depthTest_(desc.depthTest),
    depthWrite_(desc.writesDepth())
{
}

bool GlPipeline::operator==(GlPipeline const& rhs) const
{
    return program_ == rhs.program_
        && vertexSpec_ == rhs.vertexSpec_
        && blendEnabled_ == rhs.blendEnabled_
        && srcFactor_ == rhs.srcFactor_
        && dstFactor_ == rhs.dstFactor_
        && depthTest_ == rhs.depthTest_
        && depthWrite_ == rhs.depthWrite_;
}

bool GlPipeline::operator!=(GlPipeline const& rhs) const
{
    return program_ != rhs.program_
        || vertexSpec_ != rhs.vertexSpec_
        || blendEnabled_ != rhs.blendEnabled_
        || srcFactor_ != rhs.srcFactor_
        || dstFactor_ != rhs.dstFactor_
        || depthTest_ != rhs.depthTest_
        || depthWrite_ != rhs.depthWrite_;
}

bool GlPipeline::operator<(GlPipeline const& rhs) const
{
    if (blendEnabled_ != rhs.blendEnabled_)
        return !blendEnabled_;

    if (srcFactor_ != rhs.srcFactor_)
        return srcFactor_ < rhs.srcFactor_;

    if (dstFactor_ != rhs.dstFactor_)
        return dstFactor_ < rhs.dstFactor_;

    if (depthTest_ != rhs.depthTest_)
        return !depthTest_;

    if (depthWrite_ != rhs.depthWrite_)
        return !depthWrite_;

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

Format GlPipeline::getColorFormat() const
{
    return colorFormat_;
}

std::optional<Format> GlPipeline::getDepthFormat() const
{
    return depthFormat_;
}

bool GlPipeline::isBlendEnabled() const
{
    return blendEnabled_;
}

bool GlPipeline::isDepthTestEnabled() const
{
    return depthTest_;
}

bool GlPipeline::isDepthWriteEnabled() const
{
    return depthWrite_;
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
