#include "bqui/widget/introspection.h"

namespace bqui::widget
{

char const* toString(Capability capability)
{
    switch (capability)
    {
        case Capability::Clickable: return "Clickable";
        case Capability::Editable: return "Editable";
        case Capability::Focusable: return "Focusable";
        case Capability::Scrollable: return "Scrollable";
    }

    return "";
}

bool operator==(Introspection const& lhs, Introspection const& rhs)
{
    if (lhs.name != rhs.name
            || lhs.role != rhs.role
            || lhs.capabilities != rhs.capabilities
            || lhs.obb != rhs.obb
            || lhs.data != rhs.data
            || lhs.children.size() != rhs.children.size())
    {
        return false;
    }

    for (size_t i = 0; i < lhs.children.size(); ++i)
        if (*lhs.children[i] != *rhs.children[i])
            return false;

    return true;
}

Introspection resolveIntrospection(Introspection const& node,
        avg::Transform const& parent)
{
    Introspection result;
    result.name = node.name;
    result.role = node.role;
    result.capabilities = node.capabilities;
    result.obb = parent * node.obb;
    result.data = node.data;

    result.children.reserve(node.children.size());
    for (auto const& child : node.children)
        result.children.push_back(makeIntrospectionChild(
                resolveIntrospection(*child, result.obb.getTransform())));

    return result;
}

} // namespace bqui::widget
