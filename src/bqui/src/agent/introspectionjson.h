#pragma once

#include "bqui/widget/introspection.h"

#include "bqui/bquivisibility.h"

#include <nlohmann/json_fwd.hpp>

namespace bqui::agent
{
    /**
     * @brief Serialise a resolved introspection tree to a `nlohmann::json` node.
     *
     * Expects a node whose obbs are already in absolute window-space (as
     * `resolveIntrospection` / `AgentWindow::introspect` yields), so the JSON
     * carries coordinates an agent can act on directly. Each node emits its
     * `name` (omitted when unset), `role`, `capabilities` (via
     * `widget::toString`), `obb` (`center`/`size`/`angle`), `data` (recursively),
     * and `children`. Output only — there is no parser.
     *
     * A private agent-layer header: it forward-declares `nlohmann::json` only, so
     * `json.hpp` stays confined to the agent `.cpp` files. Exported so the agent
     * tests can exercise the adapter directly.
     */
    BQUI_EXPORT nlohmann::json toJson(widget::Introspection const& node);
} // namespace bqui::agent
