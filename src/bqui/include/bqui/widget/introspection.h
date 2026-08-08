#pragma once

#include "datavalue.h"

#include "bqui/bquivisibility.h"

#include <avg/obb.h>
#include <avg/transform.h>
#include <avg/rendertree/uniqueid.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bqui::widget
{
    /**
     * @brief A discoverable affordance a widget offers at its own geometry.
     *
     * A capability always applies to the `obb` of the node that carries it, so a
     * consumer knows both *what* it can do and *where*. The set grows as new
     * affordances become describable.
     */
    enum class Capability
    {
        Clickable,
        Editable,
        Focusable,
        Scrollable,
    };

    /** @brief The name of a capability, e.g. "Clickable". */
    BQUI_EXPORT char const* toString(Capability capability);

    struct Introspection;

    /** @brief An immutable, structurally shared child node. */
    using IntrospectionChild = std::shared_ptr<Introspection const>;

    /**
     * @brief A curated, introspectable description of a widget.
     *
     * A uniform, recursive node giving a consumer a faithful view of a widget:
     * its name and role, the affordances it offers, its geometry, an
     * author-controlled data bag, and its declared children. Carried on the
     * realised Instance and set through modifiers
     * (setName/setRole/setData/addCapability/setWidgetIntrospection) — a curated
     * surface, not a reflection of a widget's internals.
     *
     * `obb` is stored in the node's own **local** space; the absolute
     * window-space obbs are produced by `resolveIntrospection` at the consumer
     * boundary.
     *
     * `children` is author-controlled: opaque widgets emit none, containers
     * aggregate, and an affordance whose geometry diverges from its parent is
     * represented as a child node. Children are shared immutable nodes.
     */
    struct Introspection
    {
        std::optional<std::string> name;
        std::string role = "Widget";
        std::vector<Capability> capabilities;
        avg::Obb obb;
        DataMap data;
        std::vector<IntrospectionChild> children;

        /**
         * @brief The identity this node shares with its render-tree node.
         *
         * Set alongside the render `avg::IdNode` by `setElementId`, so a
         * consumer can join a node in the introspection tree to the same node
         * in the render/snapshot tree. Absent when the widget carries no id.
         */
        std::optional<avg::UniqueId> id;
    };

    /** @brief Wrap a node as a shared immutable child. */
    inline IntrospectionChild makeIntrospectionChild(Introspection node)
    {
        return std::make_shared<Introspection const>(std::move(node));
    }

    BQUI_EXPORT bool operator==(Introspection const& lhs,
            Introspection const& rhs);

    inline bool operator!=(Introspection const& lhs, Introspection const& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Place a node relative to an outer frame by composing `t` onto its
     * own obb.
     */
    inline Introspection transformIntrospection(Introspection node,
            avg::Transform const& t)
    {
        node.obb = t * node.obb;
        return node;
    }

    /**
     * @brief Flatten a node to absolute geometry.
     *
     * Compose `parent` onto the node's obb and, recursively, onto every
     * descendant, producing a tree whose obbs are all in the outer (window)
     * coordinate space. This is the single top-down pass that turns the shared,
     * locally-stored representation into the absolute obbs a consumer sees; run
     * it once at the boundary.
     */
    BQUI_EXPORT Introspection resolveIntrospection(Introspection const& node,
            avg::Transform const& parent = avg::Transform());
} // namespace bqui::widget
