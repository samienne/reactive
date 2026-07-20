#include "rendertree/rendertreenode.h"

#include "obb.h"
#include "transform.h"

#include <chrono>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace avg
{

RenderTreeNode::RenderTreeNode(std::optional<UniqueId> id, Animated<Obb> obb) :
    id_(id),
    obb_(std::move(obb))
{
}

std::optional<UniqueId> const& RenderTreeNode::getId() const
{
    return id_;
}

Obb RenderTreeNode::getObbAt(std::chrono::milliseconds time) const
{
    return obb_.getValue(time);
}

Animated<Obb> const& RenderTreeNode::getObb() const
{
    return obb_;
}

Obb RenderTreeNode::getFinalObb() const
{
    return obb_.getFinalValue();
}

void RenderTreeNode::transform(Transform const& transform)
{
    /*
    obb_ = Animated<Obb>(
            transform * obb_.getInitialValue(),
            transform * obb_.getFinalValue(),
            obb_.getCurve(),
            obb_.getBeginTime(),
            obb_.getDuration()
            );
    */

    std::vector<Animated<Obb>::KeyFrame> keyFrames = obb_.getKeyFrames();
    for (auto& keyFrame : keyFrames)
        keyFrame.target = transform * keyFrame.target;

    obb_ = Animated<Obb>(
            transform * obb_.getInitialValue(),
            obb_.getBeginTime(),
            std::move(keyFrames)
            );
}

} // namespace avg
