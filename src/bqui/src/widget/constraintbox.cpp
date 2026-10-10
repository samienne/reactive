#include "constraintbox.h"

#include "constraintlayout.h"

#include "bqui/widget/box.h"
#include "bqui/widget/filler.h"
#include "bqui/widget/layout.h"
#include "bqui/widget/widget.h"

#include "bqui/modifier/addwidgets.h"
#include "bqui/modifier/buildermodifier.h"
#include "bqui/modifier/setid.h"
#include "bqui/modifier/setwidgetintrospection.h"
#include "bqui/modifier/transform.h"
#include "bqui/modifier/widgetmodifier.h"

#include "bqui/provider/providebuildparams.h"
#include "bqui/provider/provideparam.h"

#include <btl/function.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <avg/obb.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <arrange/expression.h>
#include <arrange/strength.h>
#include <arrange/variable.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace bqui::widget
{

namespace
{

// Splits a child's obb into a transform and a size, places the builder, and
// gives the realised instance a fresh id so a dynamic list can match it by
// identity rather than by position. The region solution is threaded into its
// build so a nested container reads it to place its own children; a leaf
// ignores it.
bq::signal::AnySignal<widget::Instance> buildChildInRegion(
        widget::AnyBuilder const& builder,
        bq::signal::AnySignal<avg::Obb> obb,
        bq::signal::AnySignal<widget::LayoutSolution> solution)
{
    auto shared = obb.share();
    auto transform = shared.map(&avg::Obb::getTransform);
    auto size = shared.map(&avg::Obb::getSize);

    auto placed = builder.clone()
        | modifier::transformBuilder(std::move(transform));

    return (std::move(placed)(std::move(size), std::move(solution))
        | modifier::setElementId(bq::signal::constant(avg::UniqueId()))
        ).getInstance();
}

// A widget whose build receives the region solution alongside its assigned size,
// the vehicle a pure-solver container uses to place its children from the one
// solved tableau. The group f builds needs no size of its own, so the assigned
// size is handed on only to satisfy the build interface.
template <typename F>
AnyWidget makeSolutionWidget(F f)
{
    return makeWidgetFromBuilder(makeBuilder(
            [f = std::move(f)](BuildParams params,
                    bq::signal::AnySignal<avg::Vector2f> size,
                    bq::signal::AnySignal<widget::LayoutSolution> solution)
                    -> widget::AnyElement
            {
                auto sharedSize = std::move(size).share();
                AnyWidget group = f(sharedSize.clone(), std::move(solution));
                return std::move(group)(std::move(params))(sharedSize.clone());
            },
            BuildParams{},
            bq::signal::constant(avg::Vector2f(0.5f, 0.5f))));
}

void append(LayoutSpec& spec, std::vector<arrange::Constraint> constraints)
{
    for (auto& constraint : constraints)
        spec.constraints.push_back(std::move(constraint));
}

// Merges one spec's constraints and read-back variables onto another.
void appendSpec(LayoutSpec& dst, LayoutSpec const& src)
{
    dst.constraints.insert(dst.constraints.end(),
            src.constraints.begin(), src.constraints.end());
    dst.variables.insert(dst.variables.end(),
            src.variables.begin(), src.variables.end());
}

// The x half of readBackBoxes(), for the pure two-phase solve where the x-edges
// are resolved in their own pass: only the left and right of each box are read
// back out of the horizontal solution.
void readBackBoxesX(LayoutSpec& spec, std::vector<BoxVariables> const& boxes)
{
    for (BoxVariables const& box : boxes)
    {
        spec.variables.push_back(box.left);
        spec.variables.push_back(box.right);
    }
}

// The y counterpart of readBackBoxesX(): the top and bottom of each box, read
// back out of the vertical solution.
void readBackBoxesY(LayoutSpec& spec, std::vector<BoxVariables> const& boxes)
{
    for (BoxVariables const& box : boxes)
    {
        spec.variables.push_back(box.top);
        spec.variables.push_back(box.bottom);
    }
}

// The signed trailing gap G is pulled to zero a shade below the weak size
// default, so a filler (which carries no default) is dragged out to close the
// gap and fill the container, while a plain child holds its default and leaves
// the gap open rather than stretching. The pull is strictly weaker than the
// default so it never overrides one; it only decides the free extent a filler
// leaves. The gap is unbounded on both sides, so an over-full row drives it
// negative and overflows past the container's end instead of squeezing. It is
// far stronger than the flex coupling (flexCouplingStrength), so the slack keeps
// flowing to the flexing children that can still take it once others clamp at
// a bound.
arrange::Strength gapDriveStrength()
{
    return arrange::Strength::weak(0.0008);
}

// The pure main-axis tiling: consecutive children meet, the first touches the
// container's leading end, and the last is tied to the container's trailing end
// through the signed gap variable @p gap -- last.trailing + gap ==
// container.trailing, required -- which gapDriveStrength() then pulls to zero.
// No child carries a size default here; a leaf contributes its own on this axis
// and a filler its flex, so the container states only the structure (and, in
// the pure box, the flex coupling it emits per child). The gap is driven only
// when @p drive (some child flexes): with nothing to stretch the slack is left
// to the container's own size, and a drive there would only fight the parent
// stretching a filled container.
void pureMainConstraints(std::vector<arrange::Constraint>& out, Axis axis,
        BoxVariables const& container,
        std::vector<BoxVariables> const& boxes, arrange::Variable const& gap,
        bool drive)
{
    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        BoxVariables const& child = boxes[i];
        bool first = i == 0;
        bool last = i + 1 == boxes.size();

        arrange::Variable const& lead = axis == Axis::y ? child.top : child.left;
        arrange::Variable const& trail =
            axis == Axis::y ? child.bottom : child.right;
        arrange::Variable const& containerLead =
            axis == Axis::y ? container.top : container.left;
        arrange::Variable const& containerTrail =
            axis == Axis::y ? container.bottom : container.right;

        if (first)
        {
            out.push_back(arrange::Expression(lead)
                    == arrange::Expression(containerLead));
        }
        else
        {
            BoxVariables const& prev = boxes[i - 1];
            out.push_back(arrange::Expression(lead)
                    == arrange::Expression(
                        axis == Axis::y ? prev.bottom : prev.right));
        }

        if (last)
        {
            out.push_back(arrange::Expression(trail) + arrange::Expression(gap)
                    == arrange::Expression(containerTrail));
            if (drive)
            {
                out.push_back(
                        (arrange::Expression(gap) == arrange::Expression(0.0))
                        | gapDriveStrength());
            }
        }
    }
}

// One axis's worth of a pure hbox/vbox fragment, band-free. @p boxAxis selects
// the edge set this call constrains (x for left/right, y for top/bottom);
// @p layoutAxis is the container's stacking axis. On the layout axis the
// children are tiled edge to edge and the trailing slack rides @p gap
// (pureMainConstraints, driven when @p driveGap), the container stating
// structure only while each leaf or filler owns its own extent. On the cross
// axis a child that does not hold its own extent there (@p ownExtent, parallel
// to @p boxes) fills the container's cross extent. A child with a stated gravity
// (@p gravities, parallel to @p boxes) settles under it within the cross extent;
// any other is tied to the container's leading edge. The anchored outermost
// container also pins the context frame on this axis. Splitting the fragment per axis here is what lets
// E1 route the two into two disjoint solves; E0 concatenates them.
std::vector<arrange::Constraint> pureAxisConstraints(Axis boxAxis,
        Axis layoutAxis, bool anchor, BoxVariables const& container,
        std::vector<BoxVariables> const& boxes,
        std::vector<bool> const& ownExtent,
        std::vector<std::optional<avg::Vector2f>> const& gravities,
        avg::Vector2f size, arrange::Variable const& gap, bool driveGap)
{
    std::vector<arrange::Constraint> out;

    if (anchor)
    {
        if (boxAxis == Axis::x)
        {
            out.push_back(arrange::Expression(container.left)
                    == arrange::Expression(0.0));
            out.push_back(arrange::Expression(container.right)
                    == arrange::Expression(size[0]));
        }
        else
        {
            out.push_back(arrange::Expression(container.top)
                    == arrange::Expression(0.0));
            out.push_back(arrange::Expression(container.bottom)
                    == arrange::Expression(size[1]));
        }
    }

    if (boxAxis == layoutAxis)
    {
        pureMainConstraints(out, layoutAxis, container, boxes, gap, driveGap);
    }
    else
    {
        for (std::size_t i = 0; i < boxes.size(); ++i)
        {
            BoxVariables const& child = boxes[i];
            arrange::Variable const& lead =
                boxAxis == Axis::x ? child.left : child.top;
            arrange::Variable const& trail =
                boxAxis == Axis::x ? child.right : child.bottom;
            arrange::Variable const& containerLead =
                boxAxis == Axis::x ? container.left : container.top;
            arrange::Variable const& containerTrail =
                boxAxis == Axis::x ? container.right : container.bottom;

            // A child holding its own extent keeps it and is not pulled: a pull
            // it resisted would drag a flexible container down to it against
            // the slack drive. Every other child -- a flexing one or one with no
            // size of its own here -- fills the container's cross extent up to
            // its own strong max.
            bool fills = !(i < ownExtent.size() && ownExtent[i]);
            std::optional<avg::Vector2f> gravity = i < gravities.size()
                ? gravities[i] : std::nullopt;

            if (gravity)
            {
                if (fills)
                {
                    out.push_back(((arrange::Expression(trail)
                                    - arrange::Expression(lead))
                                == (arrange::Expression(containerTrail)
                                    - arrange::Expression(containerLead)))
                            | arrange::Strength::weak(1.0));
                }
                placeAtGravity(out, lead, trail, containerLead, containerTrail,
                        boxAxis == Axis::x ? gravity->x() : 1.0f - gravity->y());
                continue;
            }

            out.push_back(arrange::Expression(lead)
                    == arrange::Expression(containerLead));

            if (fills)
            {
                out.push_back((arrange::Expression(trail)
                            == arrange::Expression(containerTrail))
                        | arrange::Strength::weak(1.0));
            }
        }
    }

    return out;
}

// The whole region solves in one absolute top-down space, so a child's box is
// read out of the shared solution and expressed relative to this container's own
// solved box, then flipped into the widget tree's y-up coordinates. Subtracting
// the container's own offset lands the child parent-relative, so the enclosing
// transforms compose as usual.
std::vector<avg::Obb> regionToObbs(LayoutSolution const& solution,
        std::vector<BoxVariables> const& boxes, BoxVariables const& container)
{
    if (solution.find(container.left.id()) == solution.end())
        throw std::logic_error("bqui: a container was built outside a layout "
                "region, so no solve places its children; wrap the tree in "
                "pureSolverRoot().");

    avg::Obb containerObb = readObb(solution, container);
    avg::Vector2f containerTopLeft =
        containerObb.getTransform().getTranslation();
    float containerHeight = containerObb.getSize()[1];

    std::vector<avg::Obb> obbs;
    obbs.reserve(boxes.size());

    for (BoxVariables const& box : boxes)
    {
        avg::Obb solved = readObb(solution, box);
        avg::Vector2f childSize = solved.getSize();
        avg::Vector2f topLeft = solved.getTransform().getTranslation()
            - containerTopLeft;
        float bottomEdge = topLeft[1] + childSize[1];

        obbs.push_back(avg::Transform().translate(
                    avg::Vector2f(topLeft[0], containerHeight - bottomEdge))
                * avg::Obb(childSize));
    }

    return obbs;
}


// The layout axis the enclosing pure-solver container seeded for its fillers, as
// a signal, so its value tracks the real context a filler evaluates in rather
// than a parallel one that can diverge.
bq::signal::AnySignal<std::optional<Axis>> flexAxis(BuildParams const& params)
{
    return params.valueOrDefault<FlexAxisTag>();
}

// Builds each incoming widget once and hands the resulting builders to build.
// The builders compose their constraints for the enclosing region's solve, and
// the solution arrives as a build argument, so nothing is threaded down through
// the params but the layout axis.
AnyWidget containerLayout(
        btl::Function<AnyWidget(bq::signal::ArraySignal<widget::AnyBuilder>)>
            build,
        std::optional<std::optional<Axis>> flexAxis,
        bq::signal::ArraySignal<AnyWidget> widgets)
{
    return makeWidget([build = std::move(build), flexAxis](
                BuildParams const& params, auto widgets)
        {
            BuildParams childParams = params;

            // A stacking container seeds its layout axis, the axis its flexible
            // children publish their flex on, and a grid seeds none so they flex
            // on both; a stack seeds nothing and its children flex on the
            // enclosing box's axis.
            if (flexAxis)
                childParams.set<FlexAxisTag>(bq::signal::constant(*flexAxis));

            auto builders = widgets.map(
                    [childParams](widget::AnyWidget const& widget)
                    -> widget::AnyBuilder
                    {
                        return widget.clone()(childParams);
                    });

            return build(std::move(builders));
        },
        provider::provideBuildParams(),
        std::move(widgets)
        );
}

bq::signal::ArraySignal<widget::AnyWidget> toArray(
        std::vector<AnyWidget> widgets)
{
    std::vector<bq::signal::ArraySignal<widget::AnyWidget>> children;
    children.reserve(widgets.size());

    for (auto&& widget : widgets)
        children.push_back(std::move(widget));

    return bq::signal::ArraySignal<widget::AnyWidget>(std::move(children));
}

// The stronger of two strengths, used to hold a container's aggregate natural
// at the firmness of its firmest contributing child.
arrange::Strength strongerStrength(arrange::Strength a, arrange::Strength b)
{
    return a >= b ? a : b;
}

// Whether a band holds its widget at an extent of its own: a natural that is not
// a flex-basis, which flattenConstraints() stamps onto the box. A container
// fills a child without one to its slot and leaves one with one at its own size.
bool holdsOwnExtent(Constraints const& band)
{
    return band.natural && !(band.flex && band.flex->coeff > 0.0f);
}

// The strength of the flex coupling: far below the gap drive, so a flexing child
// clamped at a bound leaves its coupling violated rather than holding the shared
// flex variable back, and the slack keeps flowing to the children that can still
// take it.
arrange::Strength flexCouplingStrength()
{
    return arrange::Strength::weak(0.00001);
}

// The coupling that makes a flexing child a filler: its extent on the
// container's layout axis is its natural (its flex basis, zero without one) plus
// its flex weight times the container's shared flex variable, so the gap drive
// shares the slack left after every natural in proportion to the weights. Short
// of space the shared variable goes negative and the deficit is taken in the same
// proportion, each child stopping at its min (or zero) and the rest taking what
// it cannot. Emitted by the container from the band the child publishes, so a
// fixed size that cleared the flex leaves no coupling behind.
void appendFlexCoupling(LayoutSpec& spec, Constraints const& band,
        BoxVariables const& box, Axis axis, arrange::Variable const& flexShare)
{
    if (!band.flex || band.flex->coeff <= 0.0f)
        return;

    double basis = band.natural
        ? static_cast<double>(band.natural->value) : 0.0;

    spec.constraints.push_back(
            ((axis == Axis::x ? box.width() : box.height())
                == arrange::Expression(basis)
                    + static_cast<double>(band.flex->coeff)
                    * arrange::Expression(flexShare))
            | flexCouplingStrength());
}

std::vector<bool> ownExtents(std::vector<Constraints> const& bands)
{
    std::vector<bool> result;
    result.reserve(bands.size());
    for (Constraints const& band : bands)
        result.push_back(holdsOwnExtent(band));
    return result;
}

// A container's aggregate natural on one axis. On the main axis children tile
// end-to-end, so their naturals sum and the result is held at the firmest
// contributing child's strength. On the cross axis they overlap, so the largest
// wins and carries its own strength -- an unrelated smaller child's firmness
// does not rigidify the container's cross natural. Absent when no child carries
// a natural, except that a container with no children is empty content of zero
// natural rather than an unconstrained box left to the weak 100 default.
std::optional<BandNatural> aggregateNatural(
        std::vector<Constraints> const& children, bool mainAxis)
{
    if (children.empty())
        return BandNatural{ 0.0f, contentStrength() };

    std::optional<BandNatural> result;
    for (Constraints const& child : children)
    {
        if (!child.natural)
            continue;
        BandNatural const& n = *child.natural;
        if (!result)
            result = n;
        else if (mainAxis)
        {
            result = BandNatural{ result->value + n.value,
                    strongerStrength(result->strength, n.strength) };
        }
        else if (n.value > result->value)
            result = n;
    }
    return result;
}

// A container's aggregate flex on one axis. Filler coefficients sum along a
// box's main axis (fillers laid end-to-end each take a share) and take the max
// on a stack's or grid's overlaid axes (any one flexing child flexes the whole).
// Absent when no child flexes. Its presence on an axis is what makes a container
// holding a filler itself a filler to its parent there.
std::optional<Flex> aggregateFlex(
        std::vector<Constraints> const& children, bool mainAxis)
{
    std::optional<float> coeff;
    for (Constraints const& child : children)
    {
        if (!child.flex)
            continue;
        float c = child.flex->coeff;
        coeff = coeff ? (mainAxis ? *coeff + c : std::max(*coeff, c)) : c;
    }
    if (!coeff)
        return std::nullopt;
    return Flex{ *coeff };
}

// A container's aggregate min on one axis. Children tile end-to-end on the main
// axis so their mins sum; they overlap on the cross axis so the largest wins (a
// container is at least as wide as its widest child, and at least as tall as the
// sum of a column's children). Absent when no child carries a min.
std::optional<float> aggregateMin(std::vector<Constraints> const& children,
        bool mainAxis)
{
    std::optional<float> value;
    for (Constraints const& child : children)
    {
        if (!child.min)
            continue;
        value = value ? (mainAxis ? *value + *child.min
                                  : std::max(*value, *child.min))
                      : *child.min;
    }
    return value;
}

// A container's aggregate max on one axis: the sum of the children's maxes on the
// main axis, the largest on the cross axis. Absent unless every child carries a
// max, since a single uncapped child leaves the container free to grow; a cap
// taken only from the capped children would squeeze the uncapped ones.
std::optional<float> aggregateMax(std::vector<Constraints> const& children,
        bool mainAxis)
{
    std::optional<float> value;
    for (Constraints const& child : children)
    {
        if (!child.max)
            return std::nullopt;
        value = value ? (mainAxis ? *value + *child.max
                                  : std::max(*value, *child.max))
                      : *child.max;
    }
    return value;
}

// A container's aggregate min floor on one axis, published only when it flexes
// and so drops its aggregate natural. Each child contributes the extent
// below which it cannot shrink: its explicit @c min if set; otherwise, for a
// child that does not flex on this axis, its @c natural (a fixed child's natural
// is a hard floor); a flexing child can shrink to nothing and contributes zero.
// The floors aggregate as extents do -- main-axis SUM (children tile end to end),
// cross-axis MAX (children overlap) -- so a tight parent that force-sizes the
// flexing container cannot squeeze it below what its fixed content needs. Absent
// when no child floors above zero. Subsumes the explicit-min aggregate, so it
// replaces (not supplements) aggregateMin() on a flexing axis.
std::optional<float> aggregateFloor(
        std::vector<Constraints> const& children, bool mainAxis)
{
    std::optional<float> value;
    for (Constraints const& child : children)
    {
        float floor = 0.0f;
        if (child.min)
            floor = *child.min;
        else if (child.flex && child.flex->coeff > 0.0f)
            floor = 0.0f;
        else if (child.natural)
            floor = child.natural->value;
        else
            floor = 0.0f;

        if (floor <= 0.0f)
            continue;
        value = value ? (mainAxis ? *value + floor : std::max(*value, floor))
                      : floor;
    }
    return value;
}

// Composes this container's fragment with its children's onto its builder for
// the region to solve, then places its children from the solution handed to its
// build.
AnyWidget solverBoxBuilders(Axis axis,
        bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    BoxVariables container;
    arrange::Variable gap;
    arrange::Variable flexShare;

    auto boxes = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return bq::signal::AnySignal<BoxVariables>(
                            bq::signal::constant(builder.getBoxVariables()));
                })).share();

    // Each child's gravity where it states one, which places it across the box.
    auto gravities = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    using Gravity = std::optional<avg::Vector2f>;
                    if (!builder.isGravityExplicit())
                    {
                        return bq::signal::AnySignal<Gravity>(
                                bq::signal::constant(Gravity()));
                    }
                    return bq::signal::AnySignal<Gravity>(
                            builder.getGravity().map([](avg::Vector2f g)
                                {
                                    return Gravity(g);
                                }));
                })).share();

    // Each child's width band, read off its builder.
    auto childWidth = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return builder.getPureLayout().getWidth();
                }));

    // One axis of the container's published band. Each child's band is baked onto
    // its box (flattenConstraints) as untagged relations, and the aggregate
    // natural/min/max/flex is republished as the container's own band: natural and
    // the bounds aggregate main-axis SUM / cross-axis MAX, the main-axis flex
    // rides up, and a size word at this level overrides the republished band.
    auto buildAxis =
        [container, gap, flexShare](Axis thisAxis, Axis layoutAxis,
                std::vector<Constraints> const& childBands,
                std::vector<BoxVariables> const& boxes,
                std::vector<std::optional<avg::Vector2f>> const& gravities)
            -> Constraints
    {
        bool mainAxis = thisAxis == layoutAxis;

        // A child flexing along the box makes the box flexible there too, so its
        // flex rides up. Across the box a flexing child only fills the cross
        // extent the box takes from its children's naturals, so the box stays
        // rigid on that axis.
        std::optional<Flex> flex = mainAxis
            ? aggregateFlex(childBands, true) : std::nullopt;
        bool flexes = flex && flex->coeff > 0.0f;

        Constraints result;
        result.flex = flex;
        // On a flexing axis the aggregate natural is only the container's flex
        // basis: flattenConstraints() never stamps it, so it cannot inflate the
        // container, and the min is what floors its fixed content -- a fixed
        // child sets natural, not min, and aggregateFloor folds those naturals in
        // so a tight parent cannot under-allocate the container. When it does not
        // flex the stamped natural already floors it, so keep the plain
        // explicit-min aggregate there and do not double-constrain.
        result.natural = aggregateNatural(childBands, mainAxis);
        result.min = flexes
            ? aggregateFloor(childBands, mainAxis)
            : aggregateMin(childBands, mainAxis);
        result.max = aggregateMax(childBands, mainAxis);

        LayoutSpec& rel = result.relations;

        for (std::size_t i = 0; i < boxes.size() && i < childBands.size(); ++i)
        {
            appendSpec(rel, flattenConstraints(childBands[i], boxes[i],
                        thisAxis));
            if (mainAxis)
                appendFlexCoupling(rel, childBands[i], boxes[i], thisAxis,
                        flexShare);
        }

        append(rel, pureAxisConstraints(thisAxis, layoutAxis, false, container,
                    boxes, ownExtents(childBands), gravities,
                    avg::Vector2f(0.0f, 0.0f), gap, flexes));

        // The container's own weak size default, so an axis its parent neither
        // sizes nor fills (a row's height inside a column) still resolves to a
        // definite extent. Only where no child states a natural either: a
        // published natural already pins the axis, and unlike the natural (which
        // a fill() on the container drops) a default baked into the relations
        // would outlast that fill and beat the parent's gap drive, holding the
        // container at 100. Dropped on a flexing axis for the same reason.
        if (!flexes && !result.natural)
        {
            rel.constraints.push_back(thisAxis == Axis::x
                    ? weakWidthDefault(container)
                    : weakHeightDefault(container));
        }

        if (thisAxis == Axis::x)
        {
            readBackBoxesX(rel, boxes);
            rel.variables.push_back(container.left);
            rel.variables.push_back(container.right);
        }
        else
        {
            readBackBoxesY(rel, boxes);
            rel.variables.push_back(container.top);
            rel.variables.push_back(container.bottom);
        }

        return result;
    };

    auto horizontal = merge(std::move(childWidth), boxes.clone(),
            gravities.clone()).map(
            [buildAxis, axis](std::vector<Constraints> const& bands,
                    std::vector<BoxVariables> const& boxes,
                    std::vector<std::optional<avg::Vector2f>> const& gravities)
            {
                return buildAxis(Axis::x, axis, bands, boxes, gravities);
            });

    // The container's height band, as a function of the region's width solution.
    auto verticalGiven =
        [buildAxis, boxes, gravities, array, axis](
                bq::signal::AnySignal<LayoutSolution> widthSolution)
            -> bq::signal::AnySignal<Constraints>
    {
        auto childHeight = bq::signal::join(array.map(
                    [widthSolution](widget::AnyBuilder const& builder)
                    {
                        return builder.getPureLayout().getHeightForWidth(
                                widthSolution.clone());
                    }));

        return merge(std::move(childHeight), boxes.clone(), gravities.clone())
            .map(
                [buildAxis, axis](std::vector<Constraints> const& bands,
                        std::vector<BoxVariables> const& boxes,
                        std::vector<std::optional<avg::Vector2f>> const&
                            gravities)
                {
                    return buildAxis(Axis::y, axis, bands, boxes, gravities);
                });
    };

    auto widget = makeSolutionWidget(
            [container, array, boxes](
                bq::signal::AnySignal<avg::Vector2f> /*size*/,
                bq::signal::AnySignal<LayoutSolution> solution) -> AnyWidget
            {
                auto sharedSolution = std::move(solution).share();

                auto obbs = merge(sharedSolution.clone(), boxes.clone())
                    .map([container](LayoutSolution const& solution,
                                std::vector<BoxVariables> const& boxes)
                        {
                            return regionToObbs(solution, boxes, container);
                        });

                auto instances = bq::signal::join(bq::signal::scatter(
                            array, std::move(obbs),
                            [sharedSolution](widget::AnyBuilder const& builder,
                                bq::signal::AnySignal<avg::Obb> obb)
                            {
                                return buildChildInRegion(builder, std::move(obb),
                                        sharedSolution.clone());
                            }));

                return widget::makeWidget()
                    | modifier::addWidgets(std::move(instances))
                    | modifier::setRole("Layout")
                    ;
            });

    return std::move(widget)
        | modifier::makeWidgetModifier(modifier::makeBuilderModifier(
                [container, horizontal, verticalGiven](widget::AnyBuilder builder)
                {
                    builder.setBoxVariables(container);
                    builder.setPureLayout(simplePureLayout(
                            horizontal,
                            [verticalGiven](
                                bq::signal::AnySignal<LayoutSolution> ws)
                            {
                                return verticalGiven(std::move(ws));
                            }));
                    return builder;
                }))
        ;
}

// The extent a stack child's overlay fill is capped at when its band states no
// hard max: wide enough never to bind a real container, so flattenConstraints
// alone owns any genuine ceiling and the fill is left effectively uncapped.
constexpr float noSlotCap = 1.0e6f;

// The strength a stack overlays a filling child on its slot at. Below the slack
// drive (gapDriveStrength) so a child capped by its max fills up to the slot
// without dragging the slot down to the cap, leaving the slot's size to the
// flex/slack drive; a child with no cap still tracks the slot here.
arrange::Strength overlayFillStrength()
{
    return arrange::Strength::weak(0.0004);
}

// Places one child within a slot on one axis. A child holding its own extent
// (holdsOwnExtent) keeps it and settles under its gravity within the slack;
// any other child fills the slot at @p fillStrength up to its band's max (the
// flattened band owns any real ceiling, so a child stating none is uncapped).
// The vertical axis flips to y-up in the widget tree, so the child takes
// 1 - gravity.y there to keep a leading gravity leading.
void placeOnAxis(std::vector<arrange::Constraint>& out, Axis axis,
        BoxVariables const& child, arrange::Variable const& slotLead,
        arrange::Variable const& slotTrail, avg::Vector2f gravity,
        Constraints const& band, arrange::Strength fillStrength)
{
    arrange::Variable const& lead = axis == Axis::x ? child.left : child.top;
    arrange::Variable const& trail =
        axis == Axis::x ? child.right : child.bottom;
    float g = axis == Axis::x ? gravity.x() : 1.0f - gravity.y();

    if (holdsOwnExtent(band))
        placeAtGravity(out, lead, trail, slotLead, slotTrail, g);
    else
        placeInSlot(out, lead, trail, slotLead, slotTrail, g,
                band.max ? *band.max : noSlotCap, fillStrength);
}

// A stack has no layout axis and so emits no flex coupling: an inner filler
// flexes on the enclosing box's axis and fills via the weak slot pull -- the one
// real divergence from the box.
AnyWidget solverStackBuilders(
        bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    BoxVariables container;

    auto boxes = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return bq::signal::AnySignal<BoxVariables>(
                            bq::signal::constant(builder.getBoxVariables()));
                })).share();

    auto gravities = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return builder.getGravity();
                })).share();

    auto childWidth = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return builder.getPureLayout().getWidth();
                }));

    // One axis of the container's published band. Every child's band is baked
    // onto its box (flattenConstraints) and the child is overlaid on the whole
    // container slot; the aggregate natural/min/max/flex is republished as the
    // container's own band. A stack overlays on both axes, so every field
    // aggregates cross-style -- the largest child wins -- and the flex rides up
    // so a stack holding a filler is itself a filler to its parent.
    auto buildAxis =
        [container](Axis thisAxis, std::vector<Constraints> const& childBands,
                std::vector<BoxVariables> const& boxes,
                std::vector<avg::Vector2f> const& gravities) -> Constraints
    {
        std::optional<Flex> flex = aggregateFlex(childBands, false);
        // A stack treats both axes as a box treats its main axis: a flexing
        // child makes the stack flexible on that axis, whichever axis it is.
        bool flexes = flex && flex->coeff > 0.0f;

        Constraints result;
        result.flex = flex;
        // On a flexing axis the natural is only a flex basis and the min floors
        // the overlaid content (cross-style, the largest child's floor wins).
        result.natural = aggregateNatural(childBands, false);
        result.min = flexes
            ? aggregateFloor(childBands, false)
            : aggregateMin(childBands, false);
        result.max = aggregateMax(childBands, false);

        LayoutSpec& rel = result.relations;

        for (std::size_t i = 0; i < boxes.size() && i < childBands.size(); ++i)
        {
            appendSpec(rel, flattenConstraints(childBands[i], boxes[i],
                        thisAxis));

            // Overlay the child on the whole container slot.
            if (thisAxis == Axis::x)
                placeOnAxis(rel.constraints, thisAxis, boxes[i],
                        container.left, container.right, gravities[i],
                        childBands[i], overlayFillStrength());
            else
                placeOnAxis(rel.constraints, thisAxis, boxes[i],
                        container.top, container.bottom, gravities[i],
                        childBands[i], overlayFillStrength());
        }

        // The container's own weak size default, so an axis its parent neither
        // sizes nor fills still resolves to a definite extent. Dropped on a
        // flexing axis and where a child states a natural, as the pure box drops
        // it.
        if (!flexes && !result.natural)
        {
            rel.constraints.push_back(thisAxis == Axis::x
                    ? weakWidthDefault(container)
                    : weakHeightDefault(container));
        }

        if (thisAxis == Axis::x)
        {
            readBackBoxesX(rel, boxes);
            rel.variables.push_back(container.left);
            rel.variables.push_back(container.right);
        }
        else
        {
            readBackBoxesY(rel, boxes);
            rel.variables.push_back(container.top);
            rel.variables.push_back(container.bottom);
        }

        return result;
    };

    auto horizontal = merge(std::move(childWidth), boxes.clone(),
            gravities.clone()).map(
            [buildAxis](std::vector<Constraints> const& bands,
                    std::vector<BoxVariables> const& boxes,
                    std::vector<avg::Vector2f> const& gravities)
            {
                return buildAxis(Axis::x, bands, boxes, gravities);
            });

    // The container's height band, as a function of the region's width solution.
    auto verticalGiven =
        [buildAxis, boxes, gravities, array](
                bq::signal::AnySignal<LayoutSolution> widthSolution)
            -> bq::signal::AnySignal<Constraints>
    {
        auto childHeight = bq::signal::join(array.map(
                    [widthSolution](widget::AnyBuilder const& builder)
                    {
                        return builder.getPureLayout().getHeightForWidth(
                                widthSolution.clone());
                    }));

        return merge(std::move(childHeight), boxes.clone(), gravities.clone())
            .map(
                [buildAxis](std::vector<Constraints> const& bands,
                        std::vector<BoxVariables> const& boxes,
                        std::vector<avg::Vector2f> const& gravities)
                {
                    return buildAxis(Axis::y, bands, boxes, gravities);
                });
    };

    auto widget = makeSolutionWidget(
            [container, array, boxes](
                bq::signal::AnySignal<avg::Vector2f> /*size*/,
                bq::signal::AnySignal<LayoutSolution> solution) -> AnyWidget
            {
                auto sharedSolution = std::move(solution).share();

                auto obbs = merge(sharedSolution.clone(), boxes.clone())
                    .map([container](LayoutSolution const& solution,
                                std::vector<BoxVariables> const& boxes)
                        {
                            return regionToObbs(solution, boxes, container);
                        });

                auto instances = bq::signal::join(bq::signal::scatter(
                            array, std::move(obbs),
                            [sharedSolution](widget::AnyBuilder const& builder,
                                bq::signal::AnySignal<avg::Obb> obb)
                            {
                                return buildChildInRegion(builder, std::move(obb),
                                        sharedSolution.clone());
                            }));

                return widget::makeWidget()
                    | modifier::addWidgets(std::move(instances))
                    | modifier::setRole("Layout")
                    ;
            });

    return std::move(widget)
        | modifier::makeWidgetModifier(modifier::makeBuilderModifier(
                [container, horizontal, verticalGiven](widget::AnyBuilder builder)
                {
                    builder.setBoxVariables(container);
                    builder.setPureLayout(simplePureLayout(
                            horizontal,
                            [verticalGiven](
                                bq::signal::AnySignal<LayoutSolution> ws)
                            {
                                return verticalGiven(std::move(ws));
                            }));
                    return builder;
                }))
        ;
}

// Each child's band on one axis spread evenly over the tracks its cell spans:
// extents divide by the span and the flex coefficient, a weight rather than an extent, is kept whole. A child
// spanning no tracks asks nothing of the grid and is left out.
std::vector<Constraints> perTrackBands(std::vector<Constraints> const& bands,
        std::vector<GridCell> const& cells, Axis axis)
{
    std::vector<Constraints> result;
    result.reserve(bands.size());

    for (std::size_t i = 0; i < bands.size() && i < cells.size(); ++i)
    {
        GridCell const& cell = cells[i];
        if (cell.w == 0 || cell.h == 0)
            continue;

        float span = static_cast<float>(axis == Axis::x ? cell.w : cell.h);
        Constraints const& band = bands[i];

        Constraints share;
        share.flex = band.flex;
        if (band.min)
            share.min = *band.min / span;
        if (band.max)
            share.max = *band.max / span;
        if (band.natural)
            share.natural = BandNatural{ band.natural->value / span,
                band.natural->strength };
        result.push_back(std::move(share));
    }

    return result;
}

// Like the stack, a grid emits no flex coupling, so a filler in a cell fills via
// the weak slot pull. The grid lines are pinned required to equal fractions of
// the container box (gridAxisConstraints), so a cell follows the container box;
// a child holding its own extent is not pulled to its cell, so it cannot drag a
// flexing grid down to it.
AnyWidget solverGridBuilders(std::vector<GridCell> cells,
        unsigned int columns, unsigned int rows,
        bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    BoxVariables container;

    // The x lines ride the width solve and the y lines the height solve.
    GridLines lines;
    lines.xs.resize(columns + 1);
    lines.ys.resize(rows + 1);

    auto boxes = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return bq::signal::AnySignal<BoxVariables>(
                            bq::signal::constant(builder.getBoxVariables()));
                })).share();

    auto gravities = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return builder.getGravity();
                })).share();

    auto childWidth = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return builder.getPureLayout().getWidth();
                }));

    // One axis of the container's published band. Every child's band is baked
    // onto its cell box (flattenConstraints) and the child is placed within its
    // cell, bounded by the grid lines; the aggregate natural/min/max is the
    // largest per-track share (perTrackBands) of the children's, scaled by the
    // grid's dimension on this axis so a full-cell child asks the container for
    // the whole track. Flex rides up cross-style, so a grid holding a filler is
    // itself a filler to its parent.
    auto buildAxis =
        [container, lines, cells, columns, rows](Axis thisAxis,
                std::vector<Constraints> const& childBands,
                std::vector<BoxVariables> const& boxes,
                std::vector<avg::Vector2f> const& gravities) -> Constraints
    {
        float factor = static_cast<float>(
                thisAxis == Axis::x ? columns : rows);
        std::vector<Constraints> shares = perTrackBands(childBands, cells,
                thisAxis);

        std::optional<Flex> flex = aggregateFlex(shares, false);
        // Like the stack, a flexing cell child makes the grid flexible on that
        // axis, whichever axis it is.
        bool flexes = flex && flex->coeff > 0.0f;

        Constraints result;
        result.flex = flex;
        result.natural = aggregateNatural(shares, false);
        if (result.natural)
            result.natural->value *= factor;
        // On a flexing axis the natural is only a flex basis and the min floors
        // the track content; scaled by the track count like the natural, so a
        // full-cell child's floor asks for the whole track.
        result.min = flexes
            ? aggregateFloor(shares, false)
            : aggregateMin(shares, false);
        if (result.min)
            *result.min *= factor;
        result.max = aggregateMax(shares, false);
        if (result.max)
            *result.max *= factor;

        LayoutSpec& rel = result.relations;

        for (std::size_t i = 0; i < boxes.size() && i < childBands.size(); ++i)
            appendSpec(rel, flattenConstraints(childBands[i], boxes[i],
                        thisAxis));

        // Partition this axis into the grid's tracks. The x lines run
        // left..right, the y lines bottom..top in the solver's top-down space.
        if (thisAxis == Axis::x)
            gridAxisConstraints(rel.constraints, lines.xs,
                    container.left, container.right);
        else
            gridAxisConstraints(rel.constraints, lines.ys,
                    container.bottom, container.top);

        // Place each child within its cell box on this axis. The cell's
        // solver-top edge is the higher-indexed y line (ys grow bottom to top
        // while the solver runs top down), so ys[y+h] is the solver-top and
        // ys[y] the solver-bottom.
        for (std::size_t i = 0;
                i < boxes.size() && i < cells.size() && i < childBands.size();
                ++i)
        {
            GridCell const& cell = cells[i];
            if (thisAxis == Axis::x)
                placeOnAxis(rel.constraints, thisAxis, boxes[i],
                        lines.xs[cell.x], lines.xs[cell.x + cell.w],
                        gravities[i], childBands[i], arrange::Strength::weak());
            else
                placeOnAxis(rel.constraints, thisAxis, boxes[i],
                        lines.ys[cell.y + cell.h], lines.ys[cell.y],
                        gravities[i], childBands[i], arrange::Strength::weak());
        }

        // The container's own weak size default, so an axis its parent neither
        // sizes nor fills still resolves to a definite extent. Dropped on a
        // flexing axis and where a child states a natural, as the pure box and
        // stack drop it.
        if (!flexes && !result.natural)
        {
            rel.constraints.push_back(thisAxis == Axis::x
                    ? weakWidthDefault(container)
                    : weakHeightDefault(container));
        }

        if (thisAxis == Axis::x)
        {
            readBackBoxesX(rel, boxes);
            rel.variables.push_back(container.left);
            rel.variables.push_back(container.right);
        }
        else
        {
            readBackBoxesY(rel, boxes);
            rel.variables.push_back(container.top);
            rel.variables.push_back(container.bottom);
        }

        return result;
    };

    auto horizontal = merge(std::move(childWidth), boxes.clone(),
            gravities.clone()).map(
            [buildAxis](std::vector<Constraints> const& bands,
                    std::vector<BoxVariables> const& boxes,
                    std::vector<avg::Vector2f> const& gravities)
            {
                return buildAxis(Axis::x, bands, boxes, gravities);
            });

    // The container's height band, as a function of the region's width solution.
    auto verticalGiven =
        [buildAxis, boxes, gravities, array](
                bq::signal::AnySignal<LayoutSolution> widthSolution)
            -> bq::signal::AnySignal<Constraints>
    {
        auto childHeight = bq::signal::join(array.map(
                    [widthSolution](widget::AnyBuilder const& builder)
                    {
                        return builder.getPureLayout().getHeightForWidth(
                                widthSolution.clone());
                    }));

        return merge(std::move(childHeight), boxes.clone(), gravities.clone())
            .map(
                [buildAxis](std::vector<Constraints> const& bands,
                        std::vector<BoxVariables> const& boxes,
                        std::vector<avg::Vector2f> const& gravities)
                {
                    return buildAxis(Axis::y, bands, boxes, gravities);
                });
    };

    auto widget = makeSolutionWidget(
            [container, array, boxes](
                bq::signal::AnySignal<avg::Vector2f> /*size*/,
                bq::signal::AnySignal<LayoutSolution> solution) -> AnyWidget
            {
                auto sharedSolution = std::move(solution).share();

                auto obbs = merge(sharedSolution.clone(), boxes.clone())
                    .map([container](LayoutSolution const& solution,
                                std::vector<BoxVariables> const& boxes)
                        {
                            return regionToObbs(solution, boxes, container);
                        });

                auto instances = bq::signal::join(bq::signal::scatter(
                            array, std::move(obbs),
                            [sharedSolution](widget::AnyBuilder const& builder,
                                bq::signal::AnySignal<avg::Obb> obb)
                            {
                                return buildChildInRegion(builder, std::move(obb),
                                        sharedSolution.clone());
                            }));

                return widget::makeWidget()
                    | modifier::addWidgets(std::move(instances))
                    | modifier::setRole("Layout")
                    ;
            });

    return std::move(widget)
        | modifier::makeWidgetModifier(modifier::makeBuilderModifier(
                [container, horizontal, verticalGiven](widget::AnyBuilder builder)
                {
                    builder.setBoxVariables(container);
                    builder.setPureLayout(simplePureLayout(
                            horizontal,
                            [verticalGiven](
                                bq::signal::AnySignal<LayoutSolution> ws)
                            {
                                return verticalGiven(std::move(ws));
                            }));
                    return builder;
                }))
        ;
}

AnyWidget solverBox(Axis axis, bq::signal::ArraySignal<AnyWidget> widgets)
{
    return containerLayout(
            [axis](bq::signal::ArraySignal<widget::AnyBuilder> builders)
            {
                return solverBoxBuilders(axis, std::move(builders));
            },
            axis,
            std::move(widgets));
}

} // namespace

AnyWidget solverVbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return solverBox(Axis::y, std::move(widgets));
}

AnyWidget solverVbox(std::vector<AnyWidget> widgets)
{
    return solverBox(Axis::y, toArray(std::move(widgets)));
}

AnyWidget solverHbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return solverBox(Axis::x, std::move(widgets));
}

AnyWidget solverHbox(std::vector<AnyWidget> widgets)
{
    return solverBox(Axis::x, toArray(std::move(widgets)));
}

Constraints fillerAxisBand(Axis thisAxis, std::optional<Axis> layoutAxis)
{
    if (layoutAxis && thisAxis != *layoutAxis)
        return Constraints();

    Constraints c;
    c.flex = Flex{ 1.0f };
    return c;
}

namespace
{
    // The band a directional filler contributes on one axis. An axis it does not
    // fill is pinned to zero, or the gap drive / cross-fill would stretch a
    // no-extent child there; an axis it fills flexes like a plain filler when it
    // is the layout axis and is left free (cross-fill stretches it) otherwise.
    Constraints directionalFillerAxisBand(Axis thisAxis, bool fill,
            std::optional<Axis> layoutAxis)
    {
        if (!fill)
        {
            Constraints c;
            c.natural = BandNatural{ 0.0f, contentStrength() };
            return c;
        }
        return fillerAxisBand(thisAxis, layoutAxis);
    }

    // Builds a filler's PureLayout from the seeded layout-axis signal: band
    // produces one axis's Constraints from its current value, evaluated inside
    // the per-axis map so it tracks the real context.
    template <typename TBand>
    PureLayout fillerPureLayout(TBand band,
            bq::signal::AnySignal<std::optional<Axis>> axisSig)
    {
        auto sharedAxis = std::move(axisSig).share();

        auto width = sharedAxis.clone().map(
                [band](std::optional<Axis> layoutAxis)
                {
                    return band(Axis::x, layoutAxis);
                });

        WidthToConstraints heightForWidth =
            [band, sharedAxis](bq::signal::AnySignal<LayoutSolution>)
                -> bq::signal::AnySignal<Constraints>
            {
                return sharedAxis.clone().map(
                        [band](std::optional<Axis> layoutAxis)
                        {
                            return band(Axis::y, layoutAxis);
                        });
            };

        return simplePureLayout(
                bq::signal::AnySignal<Constraints>(std::move(width)),
                std::move(heightForWidth));
    }
} // namespace

AnyWidget filler()
{
    return makeWidget()
        | modifier::makeWidgetModifier(modifier::makeBuilderModifier(
                [](widget::AnyBuilder builder) -> widget::AnyBuilder
                {
                    BuildParams const& params = builder.getBuildParams();
                    builder.setPureLayout(fillerPureLayout(
                            [](Axis thisAxis, std::optional<Axis> layoutAxis)
                            {
                                return fillerAxisBand(thisAxis, layoutAxis);
                            },
                            flexAxis(params)));
                    return builder;
                }));
}

namespace
{
    AnyWidget directionalFiller(bool fillX, bool fillY)
    {
        return makeWidget()
            | modifier::makeWidgetModifier(modifier::makeBuilderModifier(
                    [fillX, fillY](widget::AnyBuilder builder)
                        -> widget::AnyBuilder
                    {
                        BuildParams const& params = builder.getBuildParams();
                        builder.setPureLayout(fillerPureLayout(
                                [fillX, fillY](Axis thisAxis,
                                    std::optional<Axis> layoutAxis)
                                {
                                    return directionalFillerAxisBand(thisAxis,
                                            thisAxis == Axis::x ? fillX : fillY,
                                            layoutAxis);
                                },
                                flexAxis(params)));
                        return builder;
                    }));
    }
} // namespace

AnyWidget hfiller()
{
    return directionalFiller(true, false);
}

AnyWidget vfiller()
{
    return directionalFiller(false, true);
}

AnyWidget hwfiller()
{
    return directionalFiller(true, true);
}

AnyWidget solverStack(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return containerLayout(
            [](bq::signal::ArraySignal<widget::AnyBuilder> builders)
            {
                return solverStackBuilders(std::move(builders));
            },
            std::nullopt,
            std::move(widgets));
}

AnyWidget solverStack(std::vector<AnyWidget> widgets)
{
    return solverStack(toArray(std::move(widgets)));
}

AnyWidget solverUniformGrid(std::vector<AnyWidget> widgets,
        std::vector<GridCell> cells, unsigned int columns, unsigned int rows)
{
    return containerLayout(
            [cells, columns, rows](
                bq::signal::ArraySignal<widget::AnyBuilder> builders)
            {
                return solverGridBuilders(cells, columns, rows,
                        std::move(builders));
            },
            std::make_optional(std::optional<Axis>()),
            toArray(std::move(widgets)));
}

PureRegion buildPureRegion(AnyWidget const& content,
        bq::signal::AnySignal<avg::Vector2f> size,
        BuildParams const& params)
{
    auto builder = content.clone()(params);
    PureLayout pure = builder.getPureLayout();
    BoxVariables root = builder.getBoxVariables();
    auto width = pure.getWidth().share();

    auto sharedSize = std::move(size).share();

    // The region owner flattens the top descriptor's band onto its outermost box
    // and alone anchors that box to the assigned rectangle; every container
    // inside states only relative structure.
    auto anchored = [root](Axis axis)
    {
        return [root, axis](Constraints const& constraints,
                avg::Vector2f size)
        {
            std::vector<LayoutSpec> all;
            all.push_back(flattenConstraints(constraints, root, axis));
            LayoutSpec anchor;
            arrange::Variable const& lead =
                axis == Axis::x ? root.left : root.top;
            arrange::Variable const& trail =
                axis == Axis::x ? root.right : root.bottom;
            float extent = axis == Axis::x ? size[0] : size[1];
            anchor.constraints.push_back(arrange::Expression(lead)
                    == arrange::Expression(0.0));
            anchor.constraints.push_back(arrange::Expression(trail)
                    == arrange::Expression(static_cast<double>(extent)));
            anchor.variables.push_back(lead);
            anchor.variables.push_back(trail);
            all.push_back(std::move(anchor));
            return all;
        };
    };

    // The width solve runs first; its solution feeds phase 2.
    auto horizontalFragments =
        merge(width.clone(), sharedSize.clone())
        .map(anchored(Axis::x));
    auto widthSolution = layoutRegion(bq::signal::AnySignal<
            std::vector<LayoutSpec>>(
            std::move(horizontalFragments))).share();

    auto heightBands = pure.getHeightForWidth(widthSolution.clone()).share();
    auto verticalFragments =
        merge(heightBands.clone(), sharedSize.clone())
        .map(anchored(Axis::y));
    auto heightSolution = layoutRegion(bq::signal::AnySignal<
            std::vector<LayoutSpec>>(
            std::move(verticalFragments)));

    auto solution = combineSolutions(widthSolution.clone(),
            std::move(heightSolution)).share();

    auto band = merge(std::move(width), std::move(heightBands)).map(
            [](Constraints width, Constraints height)
            {
                return RegionBand{ std::move(width), std::move(height) };
            });

    return PureRegion{
        std::move(builder)(sharedSize.clone(), solution.clone()),
        bq::signal::AnySignal<RegionBand>(std::move(band))
    };
}

bq::signal::AnySignal<widget::Instance> solvePureRegionAtSize(
        AnyWidget const& content,
        bq::signal::AnySignal<avg::Vector2f> size,
        BuildParams const& params)
{
    return buildPureRegion(content, std::move(size), params).element
        .getInstance();
}

AnyElement detail::buildRegionAtSize(AnyWidget const& content,
        bq::signal::AnySignal<avg::Vector2f> size,
        BuildParams const& params)
{
    return buildPureRegion(content, std::move(size), params).element;
}

AnyWidget pureSolverRoot(AnyWidget content)
{
    return makeWidgetWithSize(
            [content](bq::signal::AnySignal<avg::Vector2f> /*size*/)
            {
                return content;
            });
}

} // namespace bqui::widget
