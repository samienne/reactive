#include "constraintbox.h"

#include "constraintlayout.h"

#include "bqui/widget/filler.h"
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
                auto sharedSolution = std::move(solution).share();
                AnyWidget group = f(sharedSize.clone(), sharedSolution.clone());
                return std::move(group)(std::move(params))(sharedSize.clone(),
                        sharedSolution.clone());
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
    dst.guides.insert(dst.guides.end(),
            src.guides.begin(), src.guides.end());
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

// The signed trailing gap is pulled to zero, so the slack goes to the children
// that take it: a child free within its bounds first, then the growers, whose
// pull to their basis (growBasisStrength) is weaker. The gap is unbounded on both
// sides, so an over-full row drives it negative and overflows past the
// container's end instead of squeezing.
arrange::Strength gapDriveStrength()
{
    return arrange::Strength::weak(0.0008);
}

// The pure main-axis tiling: consecutive children meet, the first touches the
// container's leading end, and the last is tied to the container's trailing end
// through the signed gap variable @p gap -- last.trailing + gap ==
// container.trailing, required -- which gapDriveStrength() pulls to zero when
// @p drive. The drive is left out when every child holds its own extent: no
// child would take the slack, and a drive would only tie with the parent's own
// gap over a container stretched by fill().
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

// Places one child across a box on @p boxAxis. A child holding its own extent
// keeps it and is not pulled: a pull it resisted would drag a flexible
// container down to it against the slack drive. Every other child -- a flexing
// one or one with no size of its own here -- fills the container's cross extent
// up to its own strong max. Either way it settles under its gravity.
void placeAcross(std::vector<arrange::Constraint>& out, Axis boxAxis,
        BoxVariables const& container, BoxVariables const& child,
        bool ownExtent, avg::Vector2f gravity)
{
    arrange::Variable const& lead =
        boxAxis == Axis::x ? child.left : child.top;
    arrange::Variable const& trail =
        boxAxis == Axis::x ? child.right : child.bottom;
    arrange::Variable const& containerLead =
        boxAxis == Axis::x ? container.left : container.top;
    arrange::Variable const& containerTrail =
        boxAxis == Axis::x ? container.right : container.bottom;

    if (!ownExtent)
    {
        out.push_back(((arrange::Expression(trail)
                        - arrange::Expression(lead))
                    == (arrange::Expression(containerTrail)
                        - arrange::Expression(containerLead)))
                | arrange::Strength::weak(1.0));
    }
    placeAtGravity(out, lead, trail, containerLead, containerTrail,
            leadingGravity(gravity, boxAxis));
}

// One axis's worth of a pure hbox/vbox fragment, band-free. @p boxAxis selects
// the edge set this call constrains (x for left/right, y for top/bottom);
// @p layoutAxis is the container's stacking axis. On the layout axis the
// children are tiled edge to edge and the trailing slack rides @p gap
// (pureMainConstraints), the container stating
// structure only while each leaf or filler owns its own extent. On the cross
// axis a child that does not hold its own extent there (@p ownExtent, parallel
// to @p boxes) fills the container's cross extent, and every child settles
// under its gravity (@p gravities, parallel to @p boxes) within the cross
// extent.
std::vector<arrange::Constraint> pureAxisConstraints(Axis boxAxis,
        Axis layoutAxis, BoxVariables const& container,
        std::vector<BoxVariables> const& boxes,
        std::vector<bool> const& ownExtent,
        std::vector<avg::Vector2f> const& gravities,
        arrange::Variable const& gap)
{
    std::vector<arrange::Constraint> out;

    if (boxAxis == layoutAxis)
    {
        bool drive = std::find(ownExtent.begin(), ownExtent.end(), false)
            != ownExtent.end();
        pureMainConstraints(out, layoutAxis, container, boxes, gap, drive);
    }
    else
    {
        for (std::size_t i = 0; i < boxes.size(); ++i)
        {
            placeAcross(out, boxAxis, container, boxes[i],
                    i < ownExtent.size() && ownExtent[i], gravities[i]);
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
                "region, so no solve places its children; build it inside a "
                "window or a layout region.");

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


// Builds each incoming widget once and hands the resulting builders to build.
// The builders compose their constraints for the enclosing region's solve, and
// the solution arrives as a build argument, so nothing is threaded down through
// the params.
AnyWidget containerLayout(
        btl::Function<AnyWidget(bq::signal::ArraySignal<widget::AnyBuilder>)>
            build,
        bq::signal::ArraySignal<AnyWidget> widgets)
{
    return makeWidget([build = std::move(build)](
                BuildParams const& params, auto widgets)
        {
            auto builders = widgets.map(
                    [params](widget::AnyWidget const& widget)
                    -> widget::AnyBuilder
                    {
                        return widget.clone()(params);
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

// Whether a band holds its widget at an extent of its own: a natural that is not
// a flex basis, which flattenConstraints() stamps onto the box. A container
// fills a child without one to its slot and leaves one with one at its own size.
bool holdsOwnExtent(Constraints const& band)
{
    return band.natural && !(band.flex && band.flex->coeff > 0.0f);
}

// Whether a band states nothing at all on its axis: no bounds, natural or flex.
bool isNeutral(Constraints const& band)
{
    return !band.min && !band.max && !band.natural && !band.flex;
}

// The weight a child grows by along a box: its explicit flex, else 1 when its
// band is neutral (a filler), else none. A child with only bounds is not a
// grower; it is free within them and so takes slack before any grower.
std::optional<float> growWeight(Constraints const& band)
{
    if (band.flex)
    {
        if (band.flex->coeff > 0.0f)
            return band.flex->coeff;
        return std::nullopt;
    }
    if (isNeutral(band))
        return 1.0f;
    return std::nullopt;
}

// The strength of the flex coupling: the weakest pull, so a grower clamped at
// a bound leaves its coupling violated rather than holding the shared flex
// variable back, and the slack keeps flowing to the growers that can still take
// it.
arrange::Strength flexCouplingStrength()
{
    return arrange::Strength::weak(0.00001);
}

// The strength a grower is pulled to its basis at: above the coupling, so it
// does not decide the sharing, and below the gap drive, so the growers still
// take the slack. A child free within its bounds pays nothing for slack, so it
// takes the slack before any grower does.
arrange::Strength growBasisStrength()
{
    return arrange::Strength::weak(0.0001);
}

// The coupling that makes a child a grower along a box: its extent is its basis
// (its natural, zero without one) plus its weight times the container's shared
// flex variable, so the slack is shared by weight. Short of space the shared
// variable goes negative and the deficit is taken in the same proportion, each
// child stopping at its min (or zero). The basis pull makes the growers the
// last to take slack and the first to give it up. A box's growth along its own
// axis is already priced inside it, by its children and gap, so it gets none
// there and a grower costs the same however deeply it is nested.
void appendFlexCoupling(LayoutSpec& spec, Constraints const& band,
        BoxVariables const& box, Axis axis, arrange::Variable const& flexShare)
{
    std::optional<float> weight = growWeight(band);
    if (!weight)
        return;

    double basis = band.natural ? static_cast<double>(*band.natural) : 0.0;
    arrange::Expression extent = axis == Axis::x ? box.width() : box.height();

    spec.constraints.push_back((extent
                == arrange::Expression(basis)
                    + static_cast<double>(*weight)
                    * arrange::Expression(flexShare))
            | flexCouplingStrength());
    if (!band.aggregate)
    {
        spec.constraints.push_back(
                (extent == arrange::Expression(basis)) | growBasisStrength());
    }
}

std::vector<bool> ownExtents(std::vector<Constraints> const& bands)
{
    std::vector<bool> result;
    result.reserve(bands.size());
    for (Constraints const& band : bands)
        result.push_back(holdsOwnExtent(band));
    return result;
}

// The extent a child pulls toward: its natural held within its bounds, else its
// min. Absent for a child with neither.
std::optional<float> pullExtent(Constraints const& child)
{
    if (!child.natural)
        return child.min;
    float value = *child.natural;
    if (child.max)
        value = std::min(value, *child.max);
    if (child.min)
        value = std::max(value, *child.min);
    return value;
}

// A container's aggregate natural on one axis, absent when no child states one.
// Along the axis the children tile, so their pulls sum, a child without a
// natural adding its min. Across they overlap, so the largest natural wins. An
// empty container is empty content of zero natural.
std::optional<float> aggregateNatural(
        std::vector<Constraints> const& children, bool mainAxis)
{
    if (children.empty())
        return 0.0f;

    bool any = std::any_of(children.begin(), children.end(),
            [](Constraints const& child) { return child.natural.has_value(); });
    if (!any)
        return std::nullopt;

    std::optional<float> result;
    for (Constraints const& child : children)
    {
        if (mainAxis)
        {
            result = result.value_or(0.0f) + pullExtent(child).value_or(0.0f);
            continue;
        }
        if (!child.natural)
            continue;
        float value = *pullExtent(child);
        result = result ? std::max(*result, value) : value;
    }
    return result;
}

// A container's aggregate flex on one axis. Along the axis the children tile,
// so the grow weights sum (growWeight) and any grower makes the container one;
// a container of only neutral children is itself neutral and states no flex.
// Across they overlap, so any child with a flex makes the container stretch, at
// the smallest weight, bounded by the smallest max; a neutral child fills
// whatever it is given and adds nothing.
std::optional<Flex> aggregateFlex(
        std::vector<Constraints> const& children, bool mainAxis)
{
    if (mainAxis && std::all_of(children.begin(), children.end(), isNeutral))
        return std::nullopt;

    std::optional<float> coeff;
    for (Constraints const& child : children)
    {
        std::optional<float> c = mainAxis ? growWeight(child)
            : (child.flex && child.flex->coeff > 0.0f
                    ? std::make_optional(child.flex->coeff) : std::nullopt);
        if (!c)
            continue;
        coeff = coeff ? (mainAxis ? *coeff + *c : std::min(*coeff, *c)) : *c;
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

// A container's aggregate max on one axis. Along the axis the maxes sum, and a
// single uncapped child leaves the container uncapped. Across it the smallest
// cap wins, so a capped child bounds the container; uncapped children do not
// lift it, and the container is uncapped only when no child is capped.
std::optional<float> aggregateMax(std::vector<Constraints> const& children,
        bool mainAxis)
{
    std::optional<float> value;
    for (Constraints const& child : children)
    {
        if (!child.max)
        {
            if (mainAxis)
                return std::nullopt;
            continue;
        }
        value = value ? (mainAxis ? *value + *child.max
                                  : std::min(*value, *child.max))
                      : *child.max;
    }
    return value;
}

// A container's aggregate band on one axis, without its relations: flex,
// natural, min and max by the along rule on the main axis and the across rule
// otherwise. Across, a min above the smallest cap wins the cap, and the natural
// is clamped into the bounds.
Constraints aggregateBand(std::vector<Constraints> const& children,
        bool mainAxis)
{
    Constraints result;
    result.aggregate = mainAxis;
    result.flex = aggregateFlex(children, mainAxis);
    result.natural = aggregateNatural(children, mainAxis);
    result.min = aggregateMin(children, mainAxis);
    result.max = aggregateMax(children, mainAxis);

    if (!mainAxis)
    {
        if (result.min && result.max && *result.min > *result.max)
            result.max = result.min;
        if (result.natural && result.min)
            result.natural = std::max(*result.natural, *result.min);
        if (result.natural && result.max)
            result.natural = std::min(*result.natural, *result.max);
    }

    return result;
}

// The vertical axis of a baseline row, in place of the plain cross placement.
// Each child that holds its own height is aligned on the row's shared @p line by
// its baseline, or by its bottom edge when it publishes none (as CSS aligns an
// inline block without a baseline); the rest fill the row as in a plain hbox.
// The aligned block is as tall as its deepest ascent plus its deepest descent
// at the children's naturals, is placed in the row by the row's own gravity
// where the row is stamped, and its line is published as the row's own
// baseline so rows nest. Only a real baseline is published: a row of
// bottom-aligned children has none to offer.
void alignBaselines(Constraints& result,
        std::vector<Constraints> const& childBands,
        std::vector<BoxVariables> const& boxes,
        std::vector<avg::Vector2f> const& gravities,
        BoxVariables const& container, arrange::Variable const& line)
{
    std::vector<arrange::Constraint>& out = result.relations.constraints;

    std::optional<float> ascent;
    float descent = 0.0f;
    bool anyBaseline = false;

    for (std::size_t i = 0; i < boxes.size() && i < childBands.size(); ++i)
    {
        Constraints const& band = childBands[i];
        if (!holdsOwnExtent(band))
        {
            placeAcross(out, Axis::y, container, boxes[i], false,
                    gravities[i]);
            continue;
        }

        auto it = band.anchors.find(baselineAnchor.id());
        Anchor anchor = it != band.anchors.end()
            ? resolveAnchor(it->second, leadingGravity(gravities[i], Axis::y))
            : Anchor{ 1.0f, 0.0f };
        anyBaseline = anyBaseline || it != band.anchors.end();

        float natural = *band.natural;
        float childAscent = anchor.fraction * natural + anchor.offset;
        float childDescent = natural - childAscent;
        if (ascent)
        {
            ascent = std::max(*ascent, childAscent);
            descent = std::max(descent, childDescent);
        }
        else
        {
            ascent = childAscent;
            descent = childDescent;
        }

        out.push_back((arrange::Expression(boxes[i].top)
                    + static_cast<double>(anchor.fraction) * boxes[i].height()
                    + arrange::Expression(static_cast<double>(anchor.offset))
                    == arrange::Expression(line))
                | alignStrength());
    }

    if (!ascent)
        return;

    float block = *ascent + descent;

    // The line is free but for the alignments, so the placement pins it.
    result.placements.push_back(GravityPlacement{
            arrange::Expression(line)
                - arrange::Expression(static_cast<double>(*ascent)),
            block, container.top, container.bottom });

    if (!result.natural || *result.natural < block)
        result.natural = block;
    if (result.max && *result.max < block)
        result.max = block;
    if (anyBaseline)
        result.anchors[baselineAnchor.id()] = BandAnchor{
            Anchor{ 0.0f, *ascent }, Anchor{ 1.0f, -block } };
}

// A column's first baseline is its first child's. That child sits at the
// column's top, so at its natural height the anchor is a plain offset there; a
// first child without a height of its own publishes none.
void publishFirstBaseline(Constraints& result,
        std::vector<Constraints> const& childBands,
        std::vector<avg::Vector2f> const& gravities)
{
    if (childBands.empty() || gravities.empty())
        return;

    Constraints const& first = childBands.front();
    auto it = first.anchors.find(baselineAnchor.id());
    if (it == first.anchors.end() || !holdsOwnExtent(first))
        return;

    Anchor anchor = resolveAnchor(it->second,
            leadingGravity(gravities.front(), Axis::y));
    result.anchors[baselineAnchor.id()] = BandAnchor{
        Anchor{ 0.0f, anchor.fraction * *first.natural + anchor.offset },
        Anchor{} };
}

// Composes this container's fragment with its children's onto its builder for
// the region to solve, then places its children from the solution handed to its
// build. A @p baseline row aligns its children vertically on their baselines.
AnyWidget solverBoxBuilders(Axis axis, bool baseline,
        bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    BoxVariables container;
    arrange::Variable gap;
    arrange::Variable flexShare;
    arrange::Variable line;

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

    // Each child's width band, read off its builder.
    auto childWidth = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return builder.getPureLayout().getWidth();
                }));

    // One axis of the container's published band. Each child's band is baked onto
    // its box (flattenConstraints) as untagged relations, and the aggregate
    // natural/min/max/flex is republished as the container's own band
    // (aggregateBand: the along rule on the main axis, the across rule on the
    // cross axis); a size word at this level overrides the republished band.
    auto buildAxis =
        [container, gap, flexShare, line, baseline](Axis thisAxis,
                Axis layoutAxis,
                std::vector<Constraints> const& childBands,
                std::vector<BoxVariables> const& boxes,
                std::vector<avg::Vector2f> const& gravities)
            -> Constraints
    {
        bool mainAxis = thisAxis == layoutAxis;

        Constraints result = aggregateBand(childBands, mainAxis);

        LayoutSpec& rel = result.relations;

        for (std::size_t i = 0; i < boxes.size() && i < childBands.size(); ++i)
        {
            appendSpec(rel, flattenConstraints(childBands[i], boxes[i],
                        thisAxis, gravities[i]));
            if (mainAxis)
                appendFlexCoupling(rel, childBands[i], boxes[i], thisAxis,
                        flexShare);
        }

        if (baseline && thisAxis == Axis::y && layoutAxis == Axis::x)
        {
            alignBaselines(result, childBands, boxes, gravities, container,
                    line);
        }
        else
        {
            append(rel, pureAxisConstraints(thisAxis, layoutAxis, container,
                        boxes, ownExtents(childBands), gravities, gap));
        }

        if (thisAxis == Axis::y && layoutAxis == Axis::y)
            publishFirstBaseline(result, childBands, gravities);

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
                    std::vector<avg::Vector2f> const& gravities)
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
                        std::vector<avg::Vector2f> const& gravities)
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
    float g = leadingGravity(gravity, axis);

    if (holdsOwnExtent(band))
        placeAtGravity(out, lead, trail, slotLead, slotTrail, g);
    else
        placeInSlot(out, lead, trail, slotLead, slotTrail, g,
                band.max ? *band.max : noSlotCap, fillStrength);
}

// A stack has no layout axis and so emits no flex coupling: a grower fills its
// slot via the weak slot pull, as a free or neutral child does.
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
    // container's own band. A stack overlays on both axes, so both aggregate by
    // the across rule.
    auto buildAxis =
        [container](Axis thisAxis, std::vector<Constraints> const& childBands,
                std::vector<BoxVariables> const& boxes,
                std::vector<avg::Vector2f> const& gravities) -> Constraints
    {
        // The children overlap on both axes, so both take the across rule.
        Constraints result = aggregateBand(childBands, false);

        LayoutSpec& rel = result.relations;

        for (std::size_t i = 0; i < boxes.size() && i < childBands.size(); ++i)
        {
            appendSpec(rel, flattenConstraints(childBands[i], boxes[i],
                        thisAxis, gravities[i]));

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

// The band of each track on one axis: the across rule over the cells in that
// track. A child spanning several tracks is spread evenly over them -- extents
// divide by the span and the flex weight is kept whole -- and its share joins
// every track it spans. A child spanning no tracks asks nothing of the grid,
// and a track no child spans is left out.
std::vector<Constraints> trackBands(std::vector<Constraints> const& bands,
        std::vector<GridCell> const& cells, Axis axis, unsigned int count)
{
    std::vector<std::vector<Constraints>> tracks(count);

    for (std::size_t i = 0; i < bands.size() && i < cells.size(); ++i)
    {
        GridCell const& cell = cells[i];
        if (cell.w == 0 || cell.h == 0)
            continue;

        unsigned int first = axis == Axis::x ? cell.x : cell.y;
        unsigned int span = axis == Axis::x ? cell.w : cell.h;
        float divisor = static_cast<float>(span);
        Constraints const& band = bands[i];

        Constraints share;
        share.flex = band.flex;
        if (band.min)
            share.min = *band.min / divisor;
        if (band.max)
            share.max = *band.max / divisor;
        if (band.natural)
            share.natural = *band.natural / divisor;

        for (unsigned int t = first; t < first + span && t < count; ++t)
            tracks[t].push_back(share);
    }

    std::vector<Constraints> result;
    for (std::vector<Constraints> const& track : tracks)
    {
        if (!track.empty())
            result.push_back(aggregateBand(track, false));
    }
    return result;
}

// A grid's band on one axis: the along rule over its occupied @p tracks, of
// @p count in all. The grid lines hold every track to the same extent, so the
// natural and the min are the largest track's times the track count, the max
// the smallest track's times the count, and the flex weights sum as along any
// axis. An empty track holds nothing and caps nothing; a grid with no occupied
// track is empty content of zero natural.
Constraints gridAxisBand(std::vector<Constraints> const& tracks,
        unsigned int count)
{
    float n = static_cast<float>(count);

    Constraints result;
    if (tracks.empty())
    {
        result.natural = 0.0f;
        return result;
    }

    result.flex = aggregateFlex(tracks, true);
    result.natural = aggregateNatural(tracks, false);
    result.min = aggregateMin(tracks, false);
    result.max = aggregateMax(tracks, true);

    if (result.natural)
        *result.natural *= n;
    if (result.min)
        *result.min *= n;
    if (result.min && result.max && *result.min > *result.max)
        result.max = result.min;
    if (result.natural && result.max)
        result.natural = std::min(*result.natural, *result.max);

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
    // cell, bounded by the grid lines; each track takes the across rule over
    // its cells (trackBands) and the grid the along rule over its tracks
    // (gridAxisBand).
    auto buildAxis =
        [container, lines, cells, columns, rows](Axis thisAxis,
                std::vector<Constraints> const& childBands,
                std::vector<BoxVariables> const& boxes,
                std::vector<avg::Vector2f> const& gravities) -> Constraints
    {
        unsigned int count = thisAxis == Axis::x ? columns : rows;
        Constraints result = gridAxisBand(trackBands(childBands, cells,
                    thisAxis, count), count);

        LayoutSpec& rel = result.relations;

        for (std::size_t i = 0; i < boxes.size() && i < childBands.size(); ++i)
            appendSpec(rel, flattenConstraints(childBands[i], boxes[i],
                        thisAxis, gravities[i]));

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

AnyWidget solverBox(Axis axis, bool baseline,
        bq::signal::ArraySignal<AnyWidget> widgets)
{
    return containerLayout(
            [axis, baseline](
                bq::signal::ArraySignal<widget::AnyBuilder> builders)
            {
                return solverBoxBuilders(axis, baseline, std::move(builders));
            },
            std::move(widgets));
}

} // namespace

AnyWidget solverVbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return solverBox(Axis::y, false, std::move(widgets));
}

AnyWidget solverVbox(std::vector<AnyWidget> widgets)
{
    return solverBox(Axis::y, false, toArray(std::move(widgets)));
}

AnyWidget solverHbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return solverBox(Axis::x, false, std::move(widgets));
}

AnyWidget solverHbox(std::vector<AnyWidget> widgets)
{
    return solverBox(Axis::x, false, toArray(std::move(widgets)));
}

AnyWidget solverBaselineHbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return solverBox(Axis::x, true, std::move(widgets));
}

AnyWidget filler()
{
    return makeWidget();
}

namespace
{
    // An empty widget that grows on one axis and is pulled to zero on the
    // other.
    AnyWidget directionalFiller(Axis grows)
    {
        return makeWidget()
            | modifier::makeWidgetModifier(modifier::makeBuilderModifier(
                    [grows](widget::AnyBuilder builder) -> widget::AnyBuilder
                    {
                        Constraints zero;
                        zero.natural = 0.0f;
                        Constraints width = grows == Axis::x
                            ? Constraints() : zero;
                        Constraints height = grows == Axis::y
                            ? Constraints() : zero;
                        builder.setPureLayout(simplePureLayout(
                                bq::signal::constant(std::move(width)),
                                [height](bq::signal::AnySignal<LayoutSolution>)
                                    -> bq::signal::AnySignal<Constraints>
                                {
                                    return bq::signal::constant(height);
                                }));
                        return builder;
                    }));
    }
} // namespace

AnyWidget hfiller()
{
    return directionalFiller(Axis::x);
}

AnyWidget vfiller()
{
    return directionalFiller(Axis::y);
}

AnyWidget hwfiller()
{
    return filler();
}

AnyWidget solverStack(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return containerLayout(
            [](bq::signal::ArraySignal<widget::AnyBuilder> builders)
            {
                return solverStackBuilders(std::move(builders));
            },
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
    auto gravity = builder.getGravity().share();

    // The region owner alone places the outermost box, at the top-left of the
    // assigned rectangle, filling it within the box's own min and max; every
    // container inside states only relative structure. The root's natural is
    // left out, so the fill decides its extent.
    auto anchored = [root](Axis axis)
    {
        return [root, axis](Constraints const& constraints,
                avg::Vector2f size, avg::Vector2f gravity)
        {
            Constraints content;
            content.min = constraints.min;
            content.max = constraints.max;
            content.relations = constraints.relations;
            content.placements = constraints.placements;
            std::vector<LayoutSpec> all;
            all.push_back(flattenConstraints(content, root, axis, gravity));
            LayoutSpec anchor;
            arrange::Variable const& lead =
                axis == Axis::x ? root.left : root.top;
            arrange::Variable const& trail =
                axis == Axis::x ? root.right : root.bottom;
            float extent = axis == Axis::x ? size[0] : size[1];
            anchor.constraints.push_back(
                    (arrange::Expression(lead) == arrange::Expression(0.0))
                    | regionAnchorStrength());
            anchor.constraints.push_back(
                    (arrange::Expression(trail)
                        == arrange::Expression(static_cast<double>(extent)))
                    | regionFillStrength());
            anchor.variables.push_back(lead);
            anchor.variables.push_back(trail);
            all.push_back(std::move(anchor));
            return all;
        };
    };

    // The width solve runs first; its solution feeds phase 2.
    auto horizontalFragments =
        merge(width.clone(), sharedSize.clone(), gravity.clone())
        .map(anchored(Axis::x));
    auto widthSolution = layoutRegion(bq::signal::AnySignal<
            std::vector<LayoutSpec>>(
            std::move(horizontalFragments))).share();

    auto heightBands = pure.getHeightForWidth(widthSolution.clone()).share();
    auto verticalFragments =
        merge(heightBands.clone(), sharedSize.clone(), gravity.clone())
        .map(anchored(Axis::y));
    auto heightSolution = layoutRegion(bq::signal::AnySignal<
            std::vector<LayoutSpec>>(
            std::move(verticalFragments)));

    auto solution = combineSolutions(widthSolution.clone(),
            std::move(heightSolution)).share();

    auto band = merge(width.clone(), heightBands.clone()).map(
            [](Constraints width, Constraints height)
            {
                return RegionBand{ std::move(width), std::move(height) };
            });

    // The root is built at the size the solve gives it, the assigned size held
    // within its own bounds, with its top edge at the top of the region in the
    // widget tree's y-up space.
    auto clamp = [](float value, Constraints const& band)
    {
        if (band.max)
            value = std::min(value, *band.max);
        if (band.min)
            value = std::max(value, *band.min);
        return value;
    };
    auto rootSize = merge(sharedSize.clone(), width.clone(), heightBands.clone())
        .map([clamp](avg::Vector2f size, Constraints const& width,
                    Constraints const& height)
            {
                return avg::Vector2f(clamp(size[0], width),
                        clamp(size[1], height));
            }).share();
    auto rootTransform = merge(rootSize.clone(), sharedSize.clone()).map(
            [](avg::Vector2f rootSize, avg::Vector2f size)
            {
                return avg::Transform().translate(
                        avg::Vector2f(0.0f, size[1] - rootSize[1]));
            });

    auto placed = std::move(builder)
        | modifier::transformBuilder(bq::signal::AnySignal<avg::Transform>(
                    std::move(rootTransform)));

    return PureRegion{
        std::move(placed)(bq::signal::AnySignal<avg::Vector2f>(
                    std::move(rootSize)), solution.clone()),
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
