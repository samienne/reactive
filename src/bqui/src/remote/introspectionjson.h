#pragma once

#include "bqui/widget/introspection.h"

#include "bqui/bquivisibility.h"

#include <nlohmann/json_fwd.hpp>

namespace avg
{
    struct Snapshot;
} // namespace avg

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

    /**
     * @brief Serialise a render-tree snapshot to a `nlohmann::json` node.
     *
     * Emits the versioned envelope a remote client consumes: `version`, `time`,
     * `obb` and `root`, and per node `type`, `obb` (`center`/`size`/`angle`),
     * `text` and `children`, plus `id` when the node carries one and `leaving`
     * when the subtree is on its way out. Boxes are written resolved -- the size
     * is the extent the box covers, not the size it was authored with.
     * Non-finite numbers fold to zero. Output only -- there is no parser.
     */
    BQUI_EXPORT nlohmann::json toJson(avg::Snapshot const& snapshot);
} // namespace bqui::remote
