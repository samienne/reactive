#pragma once

#include "guide.h"

#include "bqui/buildparams.h"
#include "bqui/bquivisibility.h"

#include <bq/signal/constant.h>
#include <bq/signal/signal.h>

#include <avg/rendertree/uniqueid.h>

#include <map>

namespace bqui::widget
{
    /**
     * @brief A firewall's inherited guide resolutions: each resolved guide's
     * solved position keyed by the guide's stable identity.
     *
     * std::map orders by avg::UniqueId, which the guide token already compares,
     * so the same key lookup a container mints for a guide line reaches the
     * inherited value.
     */
    using ResolvedGuideMap = std::map<avg::UniqueId, float>;

    /**
     * @brief The down-channel entry a layout firewall reads to learn which
     * guides an ancestor has already resolved.
     *
     * This is the constant half of the firewall interface: a guide present in
     * the map is pinned to that fixed position inside the reading firewall's
     * solve instead of being resolved as a free coupling variable, so a guide
     * an outer container settles reaches an inner container across a
     * makeWidgetWithSize boundary as a constant and widgets on both sides line
     * up on the one line. The map accumulates on the way down: a firewall passes
     * its descendants what it received plus anything it newly resolves, so a
     * root guide threads all the way down. It carries only pre-construction
     * positions, so no cross-firewall cycle is expressible. The default is
     * empty, which is what the root firewall starts from.
     */
    struct ResolvedGuides
    {
        using type = ResolvedGuideMap;

        static bq::signal::AnySignal<ResolvedGuideMap> getDefaultValue()
        {
            return bq::signal::constant(ResolvedGuideMap());
        }
    };

    /**
     * @brief The BuildParams a root layout firewall starts from: an empty
     * resolved-guide map.
     *
     * The root is a LayoutFirewall like any other, distinguished only by being
     * fed the window size and no inherited guides. Every makeWidgetWithSize
     * boundary is a firewall too and reads the same ResolvedGuides entry, so
     * nesting composes with no special case.
     */
    inline BuildParams rootFirewallParams()
    {
        BuildParams params;
        params.set<ResolvedGuides>(
                bq::signal::constant(ResolvedGuideMap()));
        return params;
    }
} // namespace bqui::widget
