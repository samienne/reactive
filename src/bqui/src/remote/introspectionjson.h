#pragma once

#include "bqui/widget/introspection.h"

#include "bqui/bquivisibility.h"

#include <nlohmann/json_fwd.hpp>

namespace bqui::remote
{
    /**
     * @brief Serialise a resolved introspection tree to a `nlohmann::json` node.
     *
     * Expects a node whose obbs are already in absolute window-space (as
     * `resolveIntrospection` / `RemoteWindow::introspect` yields), so the JSON
     * carries coordinates a client can act on directly. Each node emits its
     * `name` (omitted when unset), `role`, `capabilities` (via
     * `widget::toString`), `obb` (`center`/`size`/`angle`), `data` (recursively),
     * and `children`. Output only — there is no parser.
     *
     * A private remote-layer header: it forward-declares `nlohmann::json` only,
     * so `json.hpp` stays confined to the remote `.cpp` files. Exported so the
     * remote tests can exercise the adapter directly.
     */
    BQUI_EXPORT nlohmann::json toJson(widget::Introspection const& node);
} // namespace bqui::remote
