#pragma once

#include "snapshot.h"

#include "avg/animated.h"
#include "avg/drawing.h"
#include "avg/transform.h"
#include "avg/drawcontext.h"
#include "avg/avgvisibility.h"

#include <optional>
#include <chrono>
#include <memory>
#include <utility>

namespace avg
{
    class RenderTreeNode;

    class AVG_EXPORT RenderTree
    {
    public:
        RenderTree();
        RenderTree(std::shared_ptr<RenderTreeNode> root);

        RenderTree(RenderTree const&) = default;
        RenderTree(RenderTree&&) noexcept = default;

        RenderTree& operator=(RenderTree const&) = default;
        RenderTree& operator=(RenderTree&&) noexcept = default;

        std::pair<RenderTree, std::optional<std::chrono::milliseconds>> update(
                RenderTree&& tree,
                std::optional<AnimationOptions> const& animationOptions,
                std::chrono::milliseconds time
                ) &&;

        std::pair<Drawing, bool> draw(DrawContext const& drawContext,
                avg::Obb const& obb,
                std::chrono::milliseconds time) const;

        /**
         * @brief Describes what the tree draws into @p obb at @p time.
         *
         * A snapshot is a pure function of an immutable tree and a timestamp,
         * so it describes exactly the frame draw() would produce for the same
         * arguments and can be taken from any thread: hold the RenderTree of
         * the frame you want, and give @p drawContext memory no other thread
         * is allocating from.
         */
        Snapshot snapshot(DrawContext const& drawContext,
                avg::Obb const& obb,
                std::chrono::milliseconds time) const;

        RenderTree transform(Transform const& transform) &&;

        std::shared_ptr<RenderTreeNode> const& getRoot() const;

    private:
        std::shared_ptr<RenderTreeNode> root_;
    };
} // namespace avg
