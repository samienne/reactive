#pragma once

#include "datavalue.h"

#include <avg/obb.h>

#include <optional>
#include <string>
#include <vector>

namespace bqui::widget
{
    /// A discoverable affordance a widget offers at its own geometry.
    ///
    /// A capability always applies to the `obb` of the node that carries it, so
    /// a consumer knows both *what* it can do and *where*. The set grows as new
    /// affordances become describable.
    enum class Capability
    {
        Clickable,
        Editable,
        Focusable,
        Scrollable,
    };

    /// A curated, introspectable description of a widget.
    ///
    /// A uniform, recursive node giving a consumer a faithful, directly usable
    /// view of a widget: its name and role, the affordances it offers, its
    /// realised window-space geometry, an author-controlled data bag, and its
    /// declared children. Carried on the realised Instance and set through
    /// modifiers (setName/setRole/setData/addCapability/setWidgetIntrospection)
    /// — a curated surface, not a reflection of a widget's internals. `obb` is
    /// the widget's actual resolved geometry, seeded from the realised size and
    /// composed to window space as the instance is placed.
    ///
    /// `children` is author-controlled: opaque widgets emit none, containers
    /// aggregate, and an affordance whose geometry diverges from its parent is
    /// represented as a child node.
    struct Introspection
    {
        std::optional<std::string> name;
        std::string role = "Widget";
        std::vector<Capability> capabilities;
        avg::Obb obb;
        DataMap data;
        std::vector<Introspection> children;
    };

    inline bool operator==(Introspection const& lhs, Introspection const& rhs)
    {
        return lhs.name == rhs.name
            && lhs.role == rhs.role
            && lhs.capabilities == rhs.capabilities
            && lhs.obb == rhs.obb
            && lhs.data == rhs.data
            && lhs.children == rhs.children;
    }

    inline bool operator!=(Introspection const& lhs, Introspection const& rhs)
    {
        return !(lhs == rhs);
    }

    /// Left-multiply every obb in the subtree by `t`, mapping the node and all
    /// its descendants into an outer coordinate space. A container uses this to
    /// lift each child's introspection from child-local into its own space; the
    /// composition applied at every layer resolves obbs to window space at the
    /// root (whose transform is the identity).
    inline Introspection transformIntrospection(Introspection node,
            avg::Transform const& t)
    {
        node.obb = t * node.obb;
        for (auto& child : node.children)
            child = transformIntrospection(std::move(child), t);
        return node;
    }
} // namespace bqui::widget
