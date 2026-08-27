#include "constraintbox.h"

#include "constraintlayout.h"

#include "bqui/widget/box.h"
#include "bqui/widget/layout.h"
#include "bqui/widget/resolvedguides.h"
#include "bqui/widget/widget.h"

#include "bqui/modifier/addwidgets.h"
#include "bqui/modifier/buildermodifier.h"
#include "bqui/modifier/setid.h"
#include "bqui/modifier/setsizehint.h"
#include "bqui/modifier/setwidgetintrospection.h"
#include "bqui/modifier/transform.h"
#include "bqui/modifier/widgetmodifier.h"

#include "bqui/provider/providebuildparams.h"
#include "bqui/provider/provideparam.h"

#include "bqui/mapsizehint.h"
#include "bqui/simplesizehint.h"
#include "bqui/sizehint.h"
#include "bqui/stacksizehint.h"

#include <btl/function.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/combine.h>
#include <bq/signal/constant.h>
#include <bq/signal/input.h>
#include <bq/signal/sharedvector.h>
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
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace bqui::widget
{

namespace
{

// The same placement step layout() uses: split a child's obb into a transform
// and a size, place the builder, and give the realised instance a fresh id so a
// dynamic list can match it by identity rather than by position.
bq::signal::AnySignal<widget::Instance> buildChild(
        widget::AnyBuilder const& builder,
        bq::signal::AnySignal<avg::Obb> obb)
{
    auto shared = obb.share();
    auto transform = shared.map(&avg::Obb::getTransform);
    auto size = shared.map(&avg::Obb::getSize);

    auto placed = builder.clone()
        | modifier::transformBuilder(std::move(transform));

    return (std::move(placed)(std::move(size))
        | modifier::setElementId(bq::signal::constant(avg::UniqueId()))
        ).getInstance();
}

void append(LayoutSpec& spec, std::vector<arrange::Constraint> constraints)
{
    for (auto& constraint : constraints)
        spec.constraints.push_back(std::move(constraint));
}

// Every child box is read back out of the solution, so all four of its edge
// variables travel with the spec.
void readBackBoxes(LayoutSpec& spec, std::vector<BoxVariables> const& boxes)
{
    for (BoxVariables const& box : boxes)
    {
        spec.variables.push_back(box.left);
        spec.variables.push_back(box.top);
        spec.variables.push_back(box.right);
        spec.variables.push_back(box.bottom);
    }
}

// Positions a baseline row's children across its cross (vertical) axis: each
// child that reports a baseline keeps its natural height and is placed so its
// baseline meets a line shared by the whole row, while a child without one falls
// back to spanning the container as a plain hbox would. The row's baseline sits
// rowAscent below its top, rowAscent being the largest child ascent, so the
// tallest ascent and the deepest descent both clear.
void appendBaselineCross(LayoutSpec& spec,
        BoxVariables const& container, avg::Vector2f size,
        std::vector<BoxVariables> const& boxes,
        std::vector<SizeHint> const& hints,
        std::vector<avg::Vector2f> const& gravities)
{
    arrange::Variable baselineLine;

    std::vector<std::optional<float>> ascents;
    std::vector<AxisHint> heights;
    ascents.reserve(boxes.size());
    heights.reserve(boxes.size());

    float rowAscent = 0.0f;
    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        float naturalWidth = hints[i].getWidthForHeight(size[1]).extent.natural;
        AxisHint height = hints[i].getHeightForWidth(naturalWidth);

        heights.push_back(height);
        ascents.push_back(height.anchors.firstBaseline);
        if (height.anchors.firstBaseline)
            rowAscent = std::max(rowAscent, *height.anchors.firstBaseline);
    }

    spec.constraints.push_back(
            arrange::Expression(baselineLine)
            == arrange::Expression(container.top)
                + arrange::Expression(rowAscent));

    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        if (!ascents[i])
        {
            // A child without a baseline falls back to the plain-row cross
            // behaviour: it fills the row up to its own height and, where it is
            // smaller, settles under its gravity within it.
            placeInSlot(spec.constraints, boxes[i].top, boxes[i].bottom,
                    container.top, container.bottom,
                    1.0f - gravities[i].y(), heights[i].extent.max);
            continue;
        }

        arrange::Expression childHeight = boxes[i].height();

        spec.constraints.push_back(
                (childHeight >= arrange::Expression(heights[i].extent.min))
                | arrange::Strength::strong());
        spec.constraints.push_back(
                (childHeight <= arrange::Expression(heights[i].extent.max))
                | arrange::Strength::strong());
        spec.constraints.push_back(
                (childHeight == arrange::Expression(heights[i].extent.natural))
                | arrange::Strength::medium());

        spec.constraints.push_back(
                (arrange::Expression(boxes[i].top)
                    + arrange::Expression(*ascents[i])
                    == arrange::Expression(baselineLine))
                | arrange::Strength::strong());
    }
}

LayoutSpec makeBoxSpec(Axis axis, BoxVariables const& container,
        arrange::Variable const& stretch, avg::Vector2f size,
        std::vector<BoxVariables> const& boxes,
        std::vector<SizeHint> const& hints,
        std::vector<std::vector<GuideAlignment>> const& alignments,
        std::vector<avg::Vector2f> const& gravities,
        ResolvedGuideMap const& resolved,
        CrossAlign crossAlign, bool anchor = true)
{
    LayoutSpec spec;

    bool baseline = axis == Axis::x && crossAlign == CrossAlign::baseline;

    if (anchor)
        append(spec, anchorConstraints(container, 0.0f, 0.0f, size[0], size[1]));
    append(spec, boxConstraints(container, boxes, axis));
    guideConstraints(spec.constraints, boxes, alignments, resolved);

    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        // The band each child is kept inside along the layout axis, measured
        // against the container's fixed cross-axis extent: heights for a
        // column, widths for a row.
        AxisHint band = axis == Axis::y
            ? hints[i].getHeightForWidth(size[0])
            : hints[i].getWidthForHeight(size[1]);
        float minExtent = band.extent.min;
        float naturalExtent = band.extent.natural;
        float maxExtent = band.extent.max;
        float grow = band.extent.grow;

        arrange::Expression childExtent = axis == Axis::y
            ? boxes[i].height()
            : boxes[i].width();

        // The band each child is kept inside, firmer than the size it settles
        // at within the band.
        spec.constraints.push_back(
                (childExtent >= arrange::Expression(minExtent))
                | arrange::Strength::strong());
        spec.constraints.push_back(
                (childExtent <= arrange::Expression(maxExtent))
                | arrange::Strength::strong());

        if (grow > 0.0f)
        {
            // A filler settles at its natural size plus its weighted share of
            // the container's one stretch variable. Every filler is tied to
            // that same variable, so the leftover space is split between them in
            // proportion to their grow weights; the container's fixed extent
            // then drives the variable to absorb the remainder.
            spec.constraints.push_back(
                    (childExtent == arrange::Expression(naturalExtent)
                        + static_cast<double>(grow) * arrange::Expression(stretch))
                    | arrange::Strength::medium());
        }
        else
        {
            spec.constraints.push_back(
                    (childExtent == arrange::Expression(naturalExtent))
                    | arrange::Strength::medium());
        }

        // Across the cross axis the child fills the container up to its own
        // maximum and, where it is smaller, sits under its gravity within the
        // slack. A baseline row instead positions the cross axis itself, so it
        // is left out here. The vertical axis flips to y-up in the widget tree,
        // so a row passes 1 - gravity.y to keep a leading gravity leading.
        if (!baseline)
        {
            AxisHint crossBand = axis == Axis::y
                ? hints[i].getWidthForHeight(size[1])
                : hints[i].getHeightForWidth(size[0]);

            if (axis == Axis::y)
                placeInSlot(spec.constraints, boxes[i].left, boxes[i].right,
                        container.left, container.right,
                        gravities[i].x(), crossBand.extent.max);
            else
                placeInSlot(spec.constraints, boxes[i].top, boxes[i].bottom,
                        container.top, container.bottom,
                        1.0f - gravities[i].y(), crossBand.extent.max);
        }
    }

    if (baseline)
        appendBaselineCross(spec, container, size, boxes, hints, gravities);

    readBackBoxes(spec, boxes);

    return spec;
}

// One axis's worth of a pure hbox/vbox fragment, band-free. @p boxAxis selects
// the edge set this call constrains (x for left/right, y for top/bottom);
// @p layoutAxis is the container's stacking axis. On the layout axis the
// children are tiled edge to edge, the shared relation a stack means; on the
// cross axis each child's leading edge is tied to the container's so its
// position is definite while its extent falls to the weak default. Every child
// carries the universal weak size default on this axis, and the anchored
// outermost container also pins the context frame on it. Splitting the fragment
// per axis here is what lets E1 route the two into two disjoint solves; E0
// concatenates them.
std::vector<arrange::Constraint> pureAxisConstraints(Axis boxAxis,
        Axis layoutAxis, bool anchor, BoxVariables const& container,
        std::vector<BoxVariables> const& boxes, avg::Vector2f size)
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
        for (auto& constraint : boxConstraints(container, boxes, layoutAxis))
            out.push_back(std::move(constraint));
    }
    else
    {
        for (BoxVariables const& child : boxes)
            out.push_back(boxAxis == Axis::x
                    ? (arrange::Expression(child.left)
                        == arrange::Expression(container.left))
                    : (arrange::Expression(child.top)
                        == arrange::Expression(container.top)));
    }

    for (BoxVariables const& child : boxes)
        out.push_back(boxAxis == Axis::x
                ? weakWidthDefault(child)
                : weakHeightDefault(child));

    return out;
}

// Sizes and positions one child within a slot on both axes: it fills the slot
// up to its own maximum and, where it is smaller, settles under its gravity
// within the slack. The slot edges are the container's for a stack and a cell's
// for a grid. The vertical axis flips to y-up in the widget tree, so the child
// takes 1 - gravity.y there to keep a leading gravity leading.
void placeChildInSlot(LayoutSpec& spec, BoxVariables const& box,
        arrange::Variable const& slotLeft, arrange::Variable const& slotTop,
        arrange::Variable const& slotRight, arrange::Variable const& slotBottom,
        SizeHint const& hint, avg::Vector2f gravity, avg::Vector2f size)
{
    float maxWidth = hint.getWidthForHeight(size[1]).extent.max;
    float maxHeight = hint.getHeightForWidth(size[0]).extent.max;

    placeInSlot(spec.constraints, box.left, box.right, slotLeft, slotRight,
            gravity.x(), maxWidth);
    placeInSlot(spec.constraints, box.top, box.bottom, slotTop, slotBottom,
            1.0f - gravity.y(), maxHeight);
}

// Every child overlays the whole container and, where it cannot use the whole
// box, settles under its gravity within it.
LayoutSpec makeStackSpec(BoxVariables const& container, avg::Vector2f size,
        std::vector<BoxVariables> const& boxes,
        std::vector<SizeHint> const& hints,
        std::vector<std::vector<GuideAlignment>> const& alignments,
        std::vector<avg::Vector2f> const& gravities,
        ResolvedGuideMap const& resolved)
{
    LayoutSpec spec;

    append(spec, anchorConstraints(container, 0.0f, 0.0f, size[0], size[1]));
    guideConstraints(spec.constraints, boxes, alignments, resolved);

    for (std::size_t i = 0; i < boxes.size(); ++i)
        placeChildInSlot(spec, boxes[i], container.left, container.top,
                container.right, container.bottom, hints[i], gravities[i], size);

    readBackBoxes(spec, boxes);

    return spec;
}

LayoutSpec makeGridSpec(BoxVariables const& container,
        std::vector<GridCell> const& cells, unsigned int columns,
        unsigned int rows, avg::Vector2f size,
        std::vector<BoxVariables> const& boxes,
        std::vector<SizeHint> const& hints,
        std::vector<std::vector<GuideAlignment>> const& alignments,
        std::vector<avg::Vector2f> const& gravities,
        ResolvedGuideMap const& resolved)
{
    LayoutSpec spec;

    append(spec, anchorConstraints(container, 0.0f, 0.0f, size[0], size[1]));
    GridLines lines = gridLines(spec.constraints, container, columns, rows);
    guideConstraints(spec.constraints, boxes, alignments, resolved);

    // Each child fills the box of its cell, bounded by the grid lines, and
    // settles under its gravity where it is smaller. The cell's top edge is the
    // higher-indexed y line (ys grow bottom to top while the solver runs top
    // down), so ys[y+h] is the solver-top and ys[y] the solver-bottom.
    for (std::size_t i = 0; i < boxes.size() && i < cells.size(); ++i)
    {
        GridCell const& cell = cells[i];
        placeChildInSlot(spec, boxes[i],
                lines.xs[cell.x], lines.ys[cell.y + cell.h],
                lines.xs[cell.x + cell.w], lines.ys[cell.y],
                hints[i], gravities[i], size);
    }

    readBackBoxes(spec, boxes);

    return spec;
}

// The stack and the grid report their band the same way multiplied hints do:
// the per-axis maximum of the children's, scaled for the grid by its
// dimensions so a full-grid child asks the container for the whole grid.
SizeHint gridSizeHint(std::vector<SizeHint> const& hints, unsigned int columns,
        unsigned int rows)
{
    float w = static_cast<float>(columns);
    float h = static_cast<float>(rows);

    auto scale = [](AxisHint hint, float factor) -> AxisHint
    {
        return AxisHint{
            Band{
                hint.extent.min * factor,
                hint.extent.natural * factor,
                hint.extent.max * factor,
                hint.extent.grow
            },
            hint.anchors
        };
    };

    return mapSizeHint(stackSizeHints(hints),
            [w, scale](AxisHint hint) -> AxisHint
            {
                return scale(hint, w);
            },
            [h, scale](AxisHint hint, float) -> AxisHint
            {
                return scale(hint, h);
            },
            [w, scale](AxisHint hint, float) -> AxisHint
            {
                return scale(hint, w);
            });
}

// The solver works in top-down window space, where y grows downwards from the
// container's top. The widget tree is y-up with the origin at the bottom-left,
// so a child's bottom edge in solver space becomes its origin here.
std::vector<avg::Obb> toObbs(LayoutSolution const& solution,
        std::vector<BoxVariables> const& boxes, avg::Vector2f size)
{
    std::vector<avg::Obb> obbs;
    obbs.reserve(boxes.size());

    for (BoxVariables const& box : boxes)
    {
        avg::Obb solved = readObb(solution, box);
        avg::Vector2f childSize = solved.getSize();
        avg::Vector2f topLeft = solved.getTransform().getTranslation();
        float bottomEdge = topLeft[1] + childSize[1];

        obbs.push_back(avg::Transform().translate(
                    avg::Vector2f(topLeft[0], size[1] - bottomEdge))
                * avg::Obb(childSize));
    }

    return obbs;
}

// The region counterpart of toObbs(): the whole region solves in one absolute
// top-down space, so a child's box is read out of the shared solution and
// expressed relative to this container's own solved box before the y flip. For
// the region's outermost container the box sits at the origin and this reduces
// to toObbs(); for a nested one it subtracts the container's own offset so the
// child lands parent-relative and the enclosing transforms compose as usual.
std::vector<avg::Obb> regionToObbs(LayoutSolution const& solution,
        std::vector<BoxVariables> const& boxes, BoxVariables const& container)
{
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

// The context a region owner threads to every participating container: a
// non-owning handle to the shared fragment collector to append to, the one
// region solution to read geometry from, and whether this container is the
// region's outermost and so anchors the coordinate origin. The collector handle
// is weak so a container node holding it does not own the collector, which owns
// the fragments the container builds — see RegionCollectorTag.
struct RegionContext
{
    std::weak_ptr<bq::signal::SharedVector<
        bq::signal::AnySignal<LayoutSpec>>> collector;
    bq::signal::AnySignal<LayoutSolution> solution;
    bool anchor;
    bool pure;
};

// Reads the region collector handle out of the build params, present only
// inside a region. The entry is a constant the region owner seeded, so
// evaluating it in its own context is safe (a constant does not diverge between
// contexts). The handle is weak; the owning reference stays with the region
// owner.
std::optional<std::weak_ptr<bq::signal::SharedVector<
    bq::signal::AnySignal<LayoutSpec>>>>
    regionCollector(BuildParams const& params)
{
    auto entry = params.get<RegionCollectorTag>();
    if (!entry)
        return std::nullopt;

    auto context = bq::signal::makeSignalContext(std::move(*entry));
    return context.evaluate<0>().get<0>();
}

bool regionAnchor(BuildParams const& params)
{
    auto context = bq::signal::makeSignalContext(
            params.valueOrDefault<RegionAnchorTag>());
    return context.evaluate<0>().get<0>();
}

// Whether the surrounding region is a pure-solver one. A constant the region
// owner seeded, so evaluating it in its own context is safe.
bool pureSolver(BuildParams const& params)
{
    auto context = bq::signal::makeSignalContext(
            params.valueOrDefault<PureSolverTag>());
    return context.evaluate<0>().get<0>();
}

// The plumbing every solver-laid-out container shares: collect the builders'
// box variables and size hints, fold one Solver over the spec makeSpec builds
// each frame, place each child at its solved rectangle, and report the
// container's own band through sizeHintMap. Only the constraints and the
// upward hint differ between a box, a stack and a grid.
template <typename MakeSpec>
AnyWidget solverLayout(MakeSpec makeSpec, SizeHintMap sizeHintMap,
        bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    auto boxes = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return bq::signal::AnySignal<BoxVariables>(
                            bq::signal::constant(builder.getBoxVariables()));
                })).share();

    auto hints = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return builder.getSizeHint();
                })).share();

    // Each child's guide alignments travel alongside its box variables and
    // hints, so the container can tie the edges they name to a shared per-guide
    // line in the same solve.
    auto alignments = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return bq::signal::AnySignal<std::vector<GuideAlignment>>(
                            bq::signal::constant(builder.getGuideAlignments()));
                })).share();

    // Each child's gravity travels the same way, so the container can place a
    // child that cannot use its whole slot within it, folding what
    // modifier::handleGravity() did as a post-pass into the solve.
    auto gravities = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return builder.getGravity();
                })).share();

    auto widget = makeWidgetWithSize(
            [makeSpec](auto size, auto resolvedGuides, auto boxes, auto hints,
                auto alignments, auto gravities, auto array)
            {
                auto sharedSize = std::move(size).share();

                auto spec = merge(sharedSize.clone(), boxes.clone(),
                        std::move(hints), std::move(alignments),
                        std::move(gravities), std::move(resolvedGuides))
                    .map([makeSpec](avg::Vector2f size,
                                std::vector<BoxVariables> const& boxes,
                                std::vector<SizeHint> const& hints,
                                std::vector<std::vector<GuideAlignment>> const&
                                    alignments,
                                std::vector<avg::Vector2f> const& gravities,
                                ResolvedGuideMap const& resolved)
                        {
                            return makeSpec(size, boxes, hints, alignments,
                                    gravities, resolved);
                        });

                auto solution = solveLayout(
                        bq::signal::AnySignal<LayoutSpec>(std::move(spec)));

                auto obbs = merge(std::move(solution), std::move(boxes),
                        std::move(sharedSize))
                    .map([](LayoutSolution const& solution,
                                std::vector<BoxVariables> const& boxes,
                                avg::Vector2f size)
                        {
                            return toObbs(solution, boxes, size);
                        });

                auto instances = bq::signal::join(bq::signal::scatter(
                            std::move(array), std::move(obbs), &buildChild));

                return widget::makeWidget()
                    | modifier::addWidgets(std::move(instances))
                    | modifier::setRole("Layout")
                    ;
            },
            provider::provideParam<ResolvedGuides>(),
            boxes,
            hints,
            alignments,
            gravities,
            std::move(array)
            );

    return std::move(widget)
        | modifier::setSizeHint(hints.map(std::move(sizeHintMap)))
        ;
}

// The region counterpart of solverLayout(): rather than folding a solve of its
// own, the container emits its spec fragment into the shared region collector
// and reads its children's geometry back out of the one region solution. Its box
// is unified with the box its parent tiles (setBoxVariables), so the outer's
// placement of this container and this container's placement of its own children
// meet on the one box in the shared tableau.
template <typename MakeSpec>
AnyWidget solverLayoutRegion(MakeSpec makeSpec, SizeHintMap sizeHintMap,
        BoxVariables container, RegionContext region,
        bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    auto boxes = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return bq::signal::AnySignal<BoxVariables>(
                            bq::signal::constant(builder.getBoxVariables()));
                })).share();

    auto hints = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return builder.getSizeHint();
                })).share();

    auto alignments = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return bq::signal::AnySignal<std::vector<GuideAlignment>>(
                            bq::signal::constant(builder.getGuideAlignments()));
                })).share();

    auto gravities = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return builder.getGravity();
                })).share();

    auto widget = makeWidgetWithSize(
            [makeSpec, container, region, sizeHintMap](auto size,
                auto resolvedGuides, auto boxes, auto hints, auto alignments,
                auto gravities, auto array)
            {
                // The fragment must not depend on the region solution: the input
                // that delivers the solution down would otherwise own, through
                // the tee, the graph that produces it — a lifetime cycle that
                // leaks the whole build. A nested container's assigned size is
                // its solved size (from the solution); its reported natural size
                // stands in instead, which the shipped size-independent bands
                // make exact. The outermost container's assigned size is the
                // region's own size, not a solved one, so it is used directly.
                bq::signal::AnySignal<avg::Vector2f> fragmentSize = region.anchor
                    ? bq::signal::AnySignal<avg::Vector2f>(std::move(size))
                    : bq::signal::AnySignal<avg::Vector2f>(hints.clone().map(
                            [sizeHintMap](std::vector<SizeHint> const& hints)
                            {
                                SizeHint hint = sizeHintMap(hints);
                                float width = hint.getWidth().extent.natural;
                                float height = hint.getHeightForWidth(width)
                                    .extent.natural;
                                return avg::Vector2f(width, height);
                            }));

                auto spec = merge(std::move(fragmentSize), boxes.clone(),
                        std::move(hints), std::move(alignments),
                        std::move(gravities), std::move(resolvedGuides))
                    .map([makeSpec](avg::Vector2f size,
                                std::vector<BoxVariables> const& boxes,
                                std::vector<SizeHint> const& hints,
                                std::vector<std::vector<GuideAlignment>> const&
                                    alignments,
                                std::vector<avg::Vector2f> const& gravities,
                                ResolvedGuideMap const& resolved)
                        {
                            return makeSpec(size, boxes, hints, alignments,
                                    gravities, resolved);
                        });

                if (auto collector = region.collector.lock())
                {
                    auto write = collector->write();
                    write->push_back(
                            bq::signal::AnySignal<LayoutSpec>(std::move(spec)));
                }

                auto obbs = merge(region.solution, std::move(boxes))
                    .map([container](LayoutSolution const& solution,
                                std::vector<BoxVariables> const& boxes)
                        {
                            return regionToObbs(solution, boxes, container);
                        });

                auto instances = bq::signal::join(bq::signal::scatter(
                            std::move(array), std::move(obbs), &buildChild));

                return widget::makeWidget()
                    | modifier::addWidgets(std::move(instances))
                    | modifier::setRole("Layout")
                    ;
            },
            provider::provideParam<ResolvedGuides>(),
            boxes,
            hints,
            alignments,
            gravities,
            std::move(array)
            );

    return std::move(widget)
        | modifier::setSizeHint(hints.map(std::move(sizeHintMap)))
        | modifier::makeWidgetModifier(modifier::makeBuilderModifier(
                [container](widget::AnyBuilder builder)
                {
                    builder.setBoxVariables(container);
                    return builder;
                }))
        ;
}

// Builds each incoming widget once and hands the resulting builders to build.
// The one place a container turns widgets into the builders its solve reads box
// variables, hints and gravity from. Each container places its children within
// their slots in the same solve (through placeInSlot()), reading each child's
// gravity off its builder, so no post-pass alignment is needed here.
AnyWidget containerLayout(
        btl::Function<AnyWidget(bq::signal::ArraySignal<widget::AnyBuilder>)>
            build,
        btl::Function<AnyWidget(bq::signal::ArraySignal<widget::AnyBuilder>,
            RegionContext)> buildRegion,
        bq::signal::ArraySignal<AnyWidget> widgets)
{
    return makeWidget([build = std::move(build),
                buildRegion = std::move(buildRegion)](BuildParams const& params,
                auto widgets)
        {
            auto collector = regionCollector(params);

            if (!collector)
            {
                auto builders = widgets.map(
                        [params](widget::AnyWidget const& widget)
                        -> widget::AnyBuilder
                        {
                            return widget.clone()(params);
                        });

                return build(std::move(builders));
            }

            RegionContext region{
                std::move(*collector),
                params.valueOrDefault<LayoutSolutionTag>(),
                regionAnchor(params),
                pureSolver(params)
            };

            // A descendant is placed by this container's tiling, not by anchoring
            // a second origin, so the anchor duty is spent here and switched off
            // below.
            BuildParams childParams = params;
            childParams.set<RegionAnchorTag>(bq::signal::constant(false));

            auto builders = widgets.map(
                    [childParams](widget::AnyWidget const& widget)
                    -> widget::AnyBuilder
                    {
                        return widget.clone()(childParams);
                    });

            return buildRegion(std::move(builders), std::move(region));
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

AnyWidget solverBoxBuilders(Axis axis, CrossAlign align,
        bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    BoxVariables container;
    arrange::Variable stretch;

    auto makeSpec = [axis, align, container, stretch](avg::Vector2f size,
            std::vector<BoxVariables> const& boxes,
            std::vector<SizeHint> const& hints,
            std::vector<std::vector<GuideAlignment>> const& alignments,
            std::vector<avg::Vector2f> const& gravities,
            ResolvedGuideMap const& resolved)
    {
        return makeBoxSpec(axis, container, stretch, size, boxes, hints,
                alignments, gravities, resolved, align);
    };

    SizeHintMap sizeHintMap = (axis == Axis::x && align == CrossAlign::baseline)
        ? SizeHintMap(accumulateBaselineRowHints)
        : (axis == Axis::y
            ? SizeHintMap(accumulateSizeHints<Axis::y>)
            : SizeHintMap(accumulateSizeHints<Axis::x>));

    return solverLayout(std::move(makeSpec), std::move(sizeHintMap),
            std::move(array));
}

AnyWidget solverBoxBuildersRegion(Axis axis, CrossAlign align,
        RegionContext region, bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    BoxVariables container;
    arrange::Variable stretch;
    bool anchor = region.anchor;

    auto makeSpec = [axis, align, container, stretch, anchor](avg::Vector2f size,
            std::vector<BoxVariables> const& boxes,
            std::vector<SizeHint> const& hints,
            std::vector<std::vector<GuideAlignment>> const& alignments,
            std::vector<avg::Vector2f> const& gravities,
            ResolvedGuideMap const& resolved)
    {
        LayoutSpec spec = makeBoxSpec(axis, container, stretch, size, boxes,
                hints, alignments, gravities, resolved, align, anchor);

        // The container reads its own box out of the region solution to place
        // its children relative to it, so those edges travel with the fragment
        // too. A per-container solve never needs this: it flips within its
        // assigned size and its box is read back by its parent instead.
        spec.variables.push_back(container.left);
        spec.variables.push_back(container.top);
        spec.variables.push_back(container.right);
        spec.variables.push_back(container.bottom);

        return spec;
    };

    SizeHintMap sizeHintMap = (axis == Axis::x && align == CrossAlign::baseline)
        ? SizeHintMap(accumulateBaselineRowHints)
        : (axis == Axis::y
            ? SizeHintMap(accumulateSizeHints<Axis::y>)
            : SizeHintMap(accumulateSizeHints<Axis::x>));

    return solverLayoutRegion(std::move(makeSpec), std::move(sizeHintMap),
            container, std::move(region), std::move(array));
}

// The pure-solver counterpart of solverBoxBuildersRegion(): the container emits
// a band-free fragment (adjacent-edge relations plus the universal weak defaults
// on every child) into the shared region collector and reads its children's
// geometry back out of the one region solution. The two axes are assembled
// separately through a BoxDescriptor so E1 can split them into two solves; E0
// concatenates them into the one combined fragment. Like the banded region
// path, its box is unified with the box its parent tiles (setBoxVariables).
AnyWidget solverBoxBuildersRegionPure(Axis axis, RegionContext region,
        bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    BoxVariables container;
    bool anchor = region.anchor;

    auto boxes = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return bq::signal::AnySignal<BoxVariables>(
                            bq::signal::constant(builder.getBoxVariables()));
                })).share();

    auto widget = makeWidgetWithSize(
            [axis, anchor, container, region](auto size, auto boxes, auto array)
            {
                // The fragment must stay off the region solution, which the solve
                // produces: only the anchored outermost container takes a real
                // size, to pin the context frame. A nested container is placed by
                // its parent's tiling, so its fragment needs no size and takes a
                // constant instead, keeping it out of the build-order cycle.
                bq::signal::AnySignal<avg::Vector2f> frame = anchor
                    ? bq::signal::AnySignal<avg::Vector2f>(std::move(size))
                    : bq::signal::AnySignal<avg::Vector2f>(
                            bq::signal::constant(avg::Vector2f(0.0f, 0.0f)));

                auto sharedFrame = std::move(frame).share();

                auto horizontal = merge(sharedFrame.clone(), boxes.clone())
                    .map([axis, anchor, container](avg::Vector2f size,
                                std::vector<BoxVariables> const& boxes)
                        {
                            return pureAxisConstraints(Axis::x, axis, anchor,
                                    container, boxes, size);
                        });

                auto verticalFrame = sharedFrame.clone();
                auto verticalBoxes = boxes.clone();
                BoxDescriptor descriptor(container,
                        BoxDescriptor::Constraints(std::move(horizontal)),
                        [axis, anchor, container, verticalFrame, verticalBoxes](
                            bq::signal::AnySignal<float>)
                        {
                            // E0 ignores the width the vertical axis is handed;
                            // E1 feeds the resolved width in here.
                            return BoxDescriptor::Constraints(
                                merge(verticalFrame.clone(),
                                        verticalBoxes.clone())
                                .map([axis, anchor, container](avg::Vector2f size,
                                            std::vector<BoxVariables> const& boxes)
                                    {
                                        return pureAxisConstraints(Axis::y, axis,
                                                anchor, container, boxes, size);
                                    }));
                        });

                // One combined solve, so the two per-axis constraint sets are
                // concatenated into the one fragment; the width fed to the
                // vertical accessor is a trivially-available zero E0 does not use.
                auto spec = merge(descriptor.getHorizontalConstraints(),
                        descriptor.getVerticalConstraints(
                            bq::signal::AnySignal<float>(
                                bq::signal::constant(0.0f))),
                        boxes.clone())
                    .map([container](
                                std::vector<arrange::Constraint> const& horizontal,
                                std::vector<arrange::Constraint> const& vertical,
                                std::vector<BoxVariables> const& boxes)
                        {
                            LayoutSpec spec;
                            append(spec, horizontal);
                            append(spec, vertical);
                            readBackBoxes(spec, boxes);

                            // The container reads its own box back to place its
                            // children relative to it, so its edges travel too.
                            spec.variables.push_back(container.left);
                            spec.variables.push_back(container.top);
                            spec.variables.push_back(container.right);
                            spec.variables.push_back(container.bottom);
                            return spec;
                        });

                if (auto collector = region.collector.lock())
                {
                    auto write = collector->write();
                    write->push_back(
                            bq::signal::AnySignal<LayoutSpec>(std::move(spec)));
                }

                auto obbs = merge(region.solution, boxes.clone())
                    .map([container](LayoutSolution const& solution,
                                std::vector<BoxVariables> const& boxes)
                        {
                            return regionToObbs(solution, boxes, container);
                        });

                auto instances = bq::signal::join(bq::signal::scatter(
                            std::move(array), std::move(obbs), &buildChild));

                return widget::makeWidget()
                    | modifier::addWidgets(std::move(instances))
                    | modifier::setRole("Layout")
                    ;
            },
            boxes,
            std::move(array)
            );

    return std::move(widget)
        | modifier::setSizeHint(bq::signal::constant(SizeHint(defaultSizeHint())))
        | modifier::makeWidgetModifier(modifier::makeBuilderModifier(
                [container](widget::AnyBuilder builder)
                {
                    builder.setBoxVariables(container);
                    return builder;
                }))
        ;
}

AnyWidget solverStackBuilders(bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    BoxVariables container;

    auto makeSpec = [container](avg::Vector2f size,
            std::vector<BoxVariables> const& boxes,
            std::vector<SizeHint> const& hints,
            std::vector<std::vector<GuideAlignment>> const& alignments,
            std::vector<avg::Vector2f> const& gravities,
            ResolvedGuideMap const& resolved)
    {
        return makeStackSpec(container, size, boxes, hints, alignments,
                gravities, resolved);
    };

    SizeHintMap sizeHintMap = [](std::vector<SizeHint> const& hints)
    {
        return SizeHint(stackSizeHints(hints));
    };

    return solverLayout(std::move(makeSpec), std::move(sizeHintMap),
            std::move(array));
}

AnyWidget solverGridBuilders(std::vector<GridCell> cells,
        unsigned int columns, unsigned int rows,
        bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    BoxVariables container;

    auto makeSpec = [container, cells, columns, rows](avg::Vector2f size,
            std::vector<BoxVariables> const& boxes,
            std::vector<SizeHint> const& hints,
            std::vector<std::vector<GuideAlignment>> const& alignments,
            std::vector<avg::Vector2f> const& gravities,
            ResolvedGuideMap const& resolved)
    {
        return makeGridSpec(container, cells, columns, rows, size, boxes,
                hints, alignments, gravities, resolved);
    };

    SizeHintMap sizeHintMap = [columns, rows](std::vector<SizeHint> const& hints)
    {
        return gridSizeHint(hints, columns, rows);
    };

    return solverLayout(std::move(makeSpec), std::move(sizeHintMap),
            std::move(array));
}

AnyWidget solverBox(Axis axis, CrossAlign align,
        bq::signal::ArraySignal<AnyWidget> widgets)
{
    return containerLayout(
            [axis, align](bq::signal::ArraySignal<widget::AnyBuilder> builders)
            {
                return solverBoxBuilders(axis, align, std::move(builders));
            },
            [axis, align](bq::signal::ArraySignal<widget::AnyBuilder> builders,
                RegionContext region)
            {
                if (region.pure)
                    return solverBoxBuildersRegionPure(axis, std::move(region),
                            std::move(builders));

                return solverBoxBuildersRegion(axis, align, std::move(region),
                        std::move(builders));
            },
            std::move(widgets));
}

AnyWidget solverBox(Axis axis, CrossAlign align, std::vector<AnyWidget> widgets)
{
    return solverBox(axis, align, toArray(std::move(widgets)));
}

} // namespace

AnyWidget solverVbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return solverBox(Axis::y, CrossAlign::fill, std::move(widgets));
}

AnyWidget solverVbox(std::vector<AnyWidget> widgets)
{
    return solverBox(Axis::y, CrossAlign::fill, std::move(widgets));
}

AnyWidget solverHbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return solverBox(Axis::x, CrossAlign::fill, std::move(widgets));
}

AnyWidget solverHbox(std::vector<AnyWidget> widgets)
{
    return solverBox(Axis::x, CrossAlign::fill, std::move(widgets));
}

AnyWidget baselineHbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return solverBox(Axis::x, CrossAlign::baseline, std::move(widgets));
}

AnyWidget baselineHbox(std::vector<AnyWidget> widgets)
{
    return solverBox(Axis::x, CrossAlign::baseline, std::move(widgets));
}

std::size_t resolvedGuideParamCount(BuildParams const& params)
{
    auto signal = provider::provideParam<ResolvedGuides>()(params);

    auto context = bq::signal::makeSignalContext(std::move(signal));

    return context.evaluate<0>().get<0>().size();
}

AnyWidget solverStack(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return containerLayout(
            [](bq::signal::ArraySignal<widget::AnyBuilder> builders)
            {
                return solverStackBuilders(std::move(builders));
            },
            [](bq::signal::ArraySignal<widget::AnyBuilder> builders,
                RegionContext)
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
    auto build = [cells, columns, rows](
            bq::signal::ArraySignal<widget::AnyBuilder> builders)
    {
        return solverGridBuilders(cells, columns, rows, std::move(builders));
    };

    return containerLayout(
            build,
            [build](bq::signal::ArraySignal<widget::AnyBuilder> builders,
                RegionContext)
            {
                return build(std::move(builders));
            },
            toArray(std::move(widgets)));
}

namespace
{

AnyWidget regionRootImpl(AnyWidget content, bool pure)
{
    return makeWidgetWithSize(
            [content, pure](auto size, BuildParams const& params)
            {
                // The solution is consumed while the region builds but produced
                // by the build; the input holds the value the tee below ties to
                // the solve, breaking the build-order cycle.
                auto input = bq::signal::makeInput(LayoutSolution());

                // The collector owns the fragment signals, which reach back to
                // the down-channel through the child builders they are built
                // from, so its owning reference must live off that path. It is
                // held here (and pinned on the region's top node below); only a
                // weak handle goes down the tree.
                auto collector = std::make_shared<bq::signal::SharedVector<
                    bq::signal::AnySignal<LayoutSpec>>>();

                BuildParams childParams = params;
                childParams.set<LayoutSolutionTag>(input.signal);
                childParams.set<RegionCollectorTag>(bq::signal::constant(
                            std::weak_ptr<bq::signal::SharedVector<
                                bq::signal::AnySignal<LayoutSpec>>>(collector)));
                childParams.set<RegionAnchorTag>(bq::signal::constant(true));
                childParams.set<PureSolverTag>(bq::signal::constant(pure));

                auto childInstance = content.clone()(childParams)(std::move(size))
                    .getInstance();

                auto fragments = collector->signal().map(
                        [](std::vector<bq::signal::AnySignal<LayoutSpec>> const&
                            parts)
                        {
                            return bq::signal::combine(parts);
                        }).join();

                auto solution = layoutRegion(
                        bq::signal::AnySignal<std::vector<LayoutSpec>>(
                            std::move(fragments)));

                auto driver = solution.tee(input.handle);

                // The map both keeps the driven solution live and pins the sole
                // owning reference to the collector, off the fragment-reachable
                // graph, so the weak down-channel handle cannot form a cycle.
                auto instance = merge(std::move(childInstance),
                        std::move(driver))
                    .map([collector](widget::Instance instance,
                                LayoutSolution const&)
                        {
                            return instance;
                        });

                return widget::makeWidget()
                    | modifier::addWidget(std::move(instance));
            },
            provider::provideBuildParams()
            );
}

} // namespace

AnyWidget regionRoot(AnyWidget content)
{
    return regionRootImpl(std::move(content), false);
}

AnyWidget pureSolverRoot(AnyWidget content)
{
    return regionRootImpl(std::move(content), true);
}

} // namespace bqui::widget
