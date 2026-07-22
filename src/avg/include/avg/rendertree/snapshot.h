#pragma once

#include "uniqueid.h"

#include "avg/avgvisibility.h"
#include "avg/drawcontext.h"
#include "avg/obb.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace avg
{
    class RenderTreeNode;

    /**
     * @brief A run of text a node draws.
     */
    struct SnapshotText
    {
        std::string text;

        /**
         * @brief The box the text occupies, in the space the snapshot was
         * taken in.
         */
        Obb obb;
    };

    /**
     * @brief One node of a render tree snapshot.
     *
     * Geometry is resolved rather than authored: @c obb is the box the node
     * occupies in the space of the obb the snapshot was taken with, so a
     * reader needs no transform of its own to place a node.
     */
    struct SnapshotNode
    {
        std::string type;
        std::optional<UniqueId> id;
        Obb obb;

        /**
         * @brief The node is still drawn but is on its way out.
         *
         * Set for a container child that has been removed and for a transition
         * that is running backwards.
         */
        bool leaving = false;

        std::vector<SnapshotText> text;
        std::vector<SnapshotNode> children;
    };

    /**
     * @brief What a render tree draws at one instant.
     *
     * Only the subtree that is drawn at @c time is described, so a transition
     * contributes the branch that is on screen and nothing else.
     */
    struct Snapshot
    {
        /**
         * @brief The version of the JSON schema avg::toJson writes.
         */
        static constexpr int version = 1;

        std::chrono::milliseconds time{ 0 };

        /**
         * @brief The box the tree was snapshotted into, usually the window.
         */
        Obb obb;

        std::optional<SnapshotNode> root;
    };

    /**
     * @brief Describes @p node itself, with neither content nor children.
     *
     * @param type The name the node is reported under.
     * @param parentObb The resolved box of the enclosing node.
     */
    AVG_EXPORT SnapshotNode makeSnapshotNode(
            std::string type,
            RenderTreeNode const& node,
            Obb const& parentObb,
            std::chrono::milliseconds time
            );

    /**
     * @brief Describes a leaf @p node along with the text it draws.
     *
     * The text is recovered by running @p node's draw function in full out of
     * @p context, which is the only place a leaf's content exists; the drawing
     * is then discarded. A snapshot therefore costs what a frame costs.
     */
    AVG_EXPORT SnapshotNode makeLeafSnapshotNode(
            std::string type,
            RenderTreeNode const& node,
            DrawContext const& context,
            Obb const& parentObb,
            std::chrono::milliseconds time
            );

    /**
     * @brief Drops the text of @p node and of its descendants that lies
     * entirely outside @p clip.
     *
     * A leaf is described on its own, so an enclosing clip has to be applied
     * afterwards for the snapshot to report only what is on screen.
     */
    AVG_EXPORT void clipSnapshotText(SnapshotNode& node, Obb const& clip);

    /**
     * @brief Serialises @p snapshot as a JSON document of schema version
     * Snapshot::version.
     *
     * A box is written resolved, so its size is the extent it covers rather
     * than the size it was authored with. Non-finite numbers are written as
     * zero.
     */
    AVG_EXPORT std::string toJson(Snapshot const& snapshot);
} // namespace avg
