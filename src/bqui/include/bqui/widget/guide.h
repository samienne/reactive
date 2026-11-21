#pragma once

#include <arrange/variable.h>

namespace bqui::widget
{
    struct GuideAccess;

    /**
     * @brief A vertical line that widgets anywhere in one layout region align
     * to on the x axis.
     *
     * A guide is a token: copies name the same line, and every widget bound to
     * it in a region shares one solved position, across sibling containers and
     * at any depth. It does not cross a size boundary, whose content is solved
     * as a region of its own. A guide nobody binds is never part of a solve.
     */
    class XGuide
    {
    public:
        XGuide() = default;

        bool operator==(XGuide const& other) const
        {
            return variable_ == other.variable_;
        }

        bool operator!=(XGuide const& other) const
        {
            return !(*this == other);
        }

    private:
        friend struct GuideAccess;

        arrange::Variable variable_;
    };

    /**
     * @brief A horizontal line that widgets anywhere in one layout region align
     * to on the y axis.
     *
     * The y counterpart of XGuide.
     */
    class YGuide
    {
    public:
        YGuide() = default;

        bool operator==(YGuide const& other) const
        {
            return variable_ == other.variable_;
        }

        bool operator!=(YGuide const& other) const
        {
            return !(*this == other);
        }

    private:
        friend struct GuideAccess;

        arrange::Variable variable_;
    };
} // namespace bqui::widget
