#pragma once

#include <optional>
#include <string>
#include <vector>

namespace bqui::widget
{
    // A curated, introspectable description of a widget: its name and role plus
    // the declared child hierarchy. Carried reactively on the Builder (see
    // builder.h) so that dynamic containers report their current contents.
    //
    // This is a deliberate, opt-in surface set through modifiers
    // (setName/setRole/setWidgetIntrospection) and aggregated by layouts — not
    // a reflection of a widget's internals.
    struct Introspection
    {
        std::optional<std::string> name;
        std::string role = "Widget";
        std::vector<Introspection> children;
    };

    inline bool operator==(Introspection const& lhs, Introspection const& rhs)
    {
        return lhs.name == rhs.name
            && lhs.role == rhs.role
            && lhs.children == rhs.children;
    }

    inline bool operator!=(Introspection const& lhs, Introspection const& rhs)
    {
        return !(lhs == rhs);
    }
} // namespace bqui::widget
