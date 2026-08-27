#include "bqui/modifier/constraintsize.h"

#include "pureconstraint.h"

#include "bqui/widget/boxvariables.h"

#include <bq/signal/constant.h>

#include <arrange/constraint.h>
#include <arrange/expression.h>
#include <arrange/strength.h>

#include <vector>

namespace bqui::modifier
{

namespace
{
    // Fill pulls a child's extent toward this large value, so it grows to
    // whatever room its container leaves. The value only has to exceed any
    // extent a real layout reaches; the container's own weak fill caps where the
    // child actually settles.
    constexpr double fillExtent = 1.0e6;

    // Fill sits above the weak 100 default so a filled child grows in preference
    // to an untagged one, and below the container's weak fill so it never pushes
    // past the room the container leaves.
    arrange::Strength fillStrength()
    {
        return arrange::Strength::weak(0.5);
    }
} // namespace

AnyWidgetModifier fillWidth()
{
    return detail::pureConstraintModifier(detail::PureAxis::horizontal,
            bq::signal::constant(0.0f),
            [](widget::BoxVariables const& box, float)
            {
                return std::vector<arrange::Constraint>{
                    (box.width() >= arrange::Expression(fillExtent))
                    | fillStrength()
                };
            });
}

AnyWidgetModifier fillHeight()
{
    return detail::pureConstraintModifier(detail::PureAxis::vertical,
            bq::signal::constant(0.0f),
            [](widget::BoxVariables const& box, float)
            {
                return std::vector<arrange::Constraint>{
                    (box.height() >= arrange::Expression(fillExtent))
                    | fillStrength()
                };
            });
}

} // namespace bqui::modifier
