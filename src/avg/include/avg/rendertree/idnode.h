#pragma once

#include "uniqueid.h"
#include "rendertreenode.h"

#include "avg/animated.h"
#include "avg/drawing.h"
#include "avg/drawcontext.h"
#include "avg/avgvisibility.h"

#include <optional>
#include <chrono>
#include <memory>
#include <typeindex>
#include <utility>

namespace avg
{
    class AVG_EXPORT IdNode : public RenderTreeNode
    {
    public:
        IdNode(
                UniqueId id,
                Animated<Obb> obb,
                std::shared_ptr<RenderTreeNode> childNode
                );

        IdNode(IdNode const&) = default;
        IdNode& operator=(IdNode const&) = default;

        UpdateResult update(
                RenderTree const& oldTree,
                RenderTree const& newTree,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                std::optional<AnimationOptions> const& animationOptions,
                std::chrono::milliseconds time
                ) const override;

        std::pair<Drawing, bool> draw(DrawContext const& context,
                avg::Obb const& obb,
                std::chrono::milliseconds time
                ) const override;

        std::type_index getType() const override;
        std::shared_ptr<RenderTreeNode> clone() const override;

    private:
        std::shared_ptr<RenderTreeNode> childNode_;
    };
} // namespace avg
