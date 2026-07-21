#pragma once

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
    class AVG_EXPORT TransitionNode : public RenderTreeNode
    {
    public:
        enum class State
        {
            transitioned,
            transitioning,
            active,
            activating
        };

        struct Transition
        {
            std::chrono::milliseconds startTime;
            std::chrono::milliseconds duration;
        };

        TransitionNode(
                Animated<Obb> obb,
                bool isActive,
                std::shared_ptr<RenderTreeNode> activeNode,
                std::shared_ptr<RenderTreeNode> transitionedNode
                );

        TransitionNode(TransitionNode const&) = default;
        TransitionNode& operator=(TransitionNode const&) = default;

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
        std::shared_ptr<RenderTreeNode> activeNode_;
        std::shared_ptr<RenderTreeNode> transitionedNode_;
        std::optional<Transition> transition_;
        bool isActive_ = true;
    };
} // namespace avg
