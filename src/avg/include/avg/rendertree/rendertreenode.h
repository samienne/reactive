#pragma once

#include "snapshot.h"
#include "uniqueid.h"
#include "updateresult.h"

#include "avg/animated.h"
#include "avg/drawing.h"
#include "avg/transform.h"
#include "avg/drawcontext.h"
#include "avg/avgvisibility.h"

#include <optional>
#include <chrono>
#include <memory>
#include <typeindex>
#include <utility>

namespace avg
{
    class RenderTree;

    class AVG_EXPORT RenderTreeNode
    {
    public:
        RenderTreeNode(
                std::optional<UniqueId> id,
                Animated<Obb> obb
                );

        virtual ~RenderTreeNode() = default;

        std::optional<UniqueId> const& getId() const;
        Obb getObbAt(std::chrono::milliseconds time) const;
        Animated<Obb> const& getObb() const;
        Obb getFinalObb() const;

        void transform(Transform const& transform);

        virtual UpdateResult update(
                RenderTree const& oldTree,
                RenderTree const& newTree,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                std::optional<AnimationOptions> const& animationOptions,
                std::chrono::milliseconds time
                ) const = 0;

        virtual std::pair<Drawing, bool> draw(DrawContext const& context,
                avg::Obb const& obb,
                std::chrono::milliseconds time
                ) const = 0;

        /**
         * @brief Describes what this node draws at @p time.
         *
         * Reports the same subtree draw() would visit, resolved into the space
         * of @p parentObb. Leaves the tree unchanged; @p context's memory must
         * not be shared with a concurrent draw.
         */
        virtual SnapshotNode snapshot(
                DrawContext const& context,
                avg::Obb const& parentObb,
                std::chrono::milliseconds time
                ) const = 0;

        virtual std::type_index getType() const = 0;
        virtual std::shared_ptr<RenderTreeNode> clone() const = 0;

    private:
        std::optional<UniqueId> id_;
        Animated<Obb> obb_;
    };

    /**
     * @brief Whether update() can carry @p oldNode over into @p newNode.
     *
     * Only nodes with the same id and of the same kind can; any other pair is
     * one node leaving and another arriving.
     */
    AVG_EXPORT bool canUpdate(RenderTreeNode const& oldNode,
            RenderTreeNode const& newNode);

    /**
     * @brief Updates the one node a place in the tree holds from @p oldNode
     * to @p newNode, either of which may be null.
     *
     * When canUpdate() fails, the old node is dropped and the new one
     * appears.
     */
    AVG_EXPORT UpdateResult updateNode(
            RenderTree const& oldTree,
            RenderTree const& newTree,
            std::shared_ptr<RenderTreeNode> const& oldNode,
            std::shared_ptr<RenderTreeNode> const& newNode,
            std::optional<AnimationOptions> const& animationOptions,
            std::chrono::milliseconds time
            );
} // namespace avg
