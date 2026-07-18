#pragma once

#include "bqui/widget/introspection.h"

#include "bqui/bquivisibility.h"

#include <string>

namespace bqui::agent
{
    /**
     * @brief Serialise a resolved introspection tree to a JSON string.
     *
     * Expects a node whose obbs are already in absolute window-space (as
     * `resolveIntrospection` / `Element::getIntrospection` yields), so the JSON
     * carries coordinates an agent can act on directly. Each node emits its
     * `name` (omitted when unset), `role`, `capabilities`, `obb`
     * (`center`/`size`/`angle`), `data`, and `children`. Output only — there is
     * no parser.
     */
    BQUI_EXPORT std::string toJson(widget::Introspection const& node);
} // namespace bqui::agent
