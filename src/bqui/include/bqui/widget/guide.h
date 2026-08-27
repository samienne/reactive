#pragma once

#include "bqui/bquivisibility.h"

#include <avg/rendertree/uniqueid.h>

#include <vector>

namespace bqui::widget
{
    struct GuideAccess;

    /**
     * @brief A shared vertical guide line the user aligns widgets to on the X
     * axis.
     *
     * An XGuide is an opaque token minted when the widgets are defined, so its
     * identity is stable and shareable: construct one and align several widgets'
     * left, right or horizontal-centre edges to it, and their container lines
     * those edges up on one solved X position. The axis lives in the type — an
     * XGuide only positions on X — so aligning a horizontal edge to a Y guide is
     * a compile error. Copying preserves identity; two copies name the same
     * line. There is no id getter: the layout internals reach the identity
     * through GuideAccess.
     */
    class XGuide
    {
    public:
        XGuide() = default;

        XGuide(XGuide const&) = default;
        XGuide& operator=(XGuide const&) = default;

        bool operator==(XGuide const& other) const
        {
            return id_ == other.id_;
        }

        bool operator!=(XGuide const& other) const
        {
            return !(*this == other);
        }

    private:
        friend struct GuideAccess;

        avg::UniqueId id_;
    };

    /**
     * @brief A shared horizontal guide line the user aligns widgets to on the Y
     * axis.
     *
     * The Y counterpart of XGuide: align widgets' top, bottom or
     * vertical-centre edges to it and their container lines those edges up on
     * one solved Y position. The axis lives in the type, so a YGuide only
     * positions on Y.
     */
    class YGuide
    {
    public:
        YGuide() = default;

        YGuide(YGuide const&) = default;
        YGuide& operator=(YGuide const&) = default;

        bool operator==(YGuide const& other) const
        {
            return id_ == other.id_;
        }

        bool operator!=(YGuide const& other) const
        {
            return !(*this == other);
        }

    private:
        friend struct GuideAccess;

        avg::UniqueId id_;
    };

    /**
     * @brief Which edge of a widget's box a guide alignment pins to the guide
     * line.
     */
    enum class GuideEdge
    {
        left,
        right,
        centerX,
        top,
        bottom,
        centerY
    };

    /**
     * @brief One widget's request to line one of its edges up on a guide.
     *
     * A widget exposes its alignments for its container to read, parallel to its
     * box variables. @c guide is the guide's stable identity (an XGuide's or a
     * YGuide's), matched across the container's children so every widget aligned
     * to the same guide shares one solved line; @c edge names the box edge that
     * meets it.
     */
    struct GuideAlignment
    {
        avg::UniqueId guide;
        GuideEdge edge;
    };
} // namespace bqui::widget
