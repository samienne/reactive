#include "constraintlayout.h"

#include "bqui/widget/builder.h"

#include <avg/rendertree/uniqueid.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <arrange/errors.h>
#include <arrange/solver.h>
#include <arrange/strength.h>

#include <bq/signal/constant.h>
#include <bq/signal/merge.h>
#include <bq/signal/signal.h>

#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace bqui::widget
{

namespace
{
    // The solver rides through the fold as accumulator state alongside the
    // snapshot it produced. Only the snapshot is mapped out and shared, so the
    // tableau is never copied at a shared node.
    struct SolveState
    {
        arrange::Solver solver;
        LayoutSolution solution;
    };

    float valueOf(LayoutSolution const& solution, arrange::Variable const& v)
    {
        auto it = solution.find(v.id());
        return it != solution.end() ? static_cast<float>(it->second) : 0.0f;
    }

    arrange::Constraint pin(arrange::Variable const& a, arrange::Variable const& b)
    {
        return arrange::Expression(a) == arrange::Expression(b);
    }
} // namespace

namespace
{
    // An empty descriptor: no bands and no relations on either axis. The height
    // phase ignores its width argument, as every widget's does this increment.
    PureLayout emptyPureLayout()
    {
        return PureLayout{
            bq::signal::constant(Constraints()),
            [](bq::signal::AnySignal<float>)
            {
                return bq::signal::AnySignal<Constraints>(
                        bq::signal::constant(Constraints()));
            }
        };
    }

    PureLayout pureLayoutOr(AnyBuilder const& builder)
    {
        std::optional<PureLayout> current = builder.getPureLayout();
        return current ? *current : emptyPureLayout();
    }

    void appendSpec(LayoutSpec& dst, LayoutSpec const& src)
    {
        dst.constraints.insert(dst.constraints.end(),
                src.constraints.begin(), src.constraints.end());
        dst.variables.insert(dst.variables.end(),
                src.variables.begin(), src.variables.end());
    }

    // Maps the axis's Constraints signal through @p apply, wrapping the height
    // phase function so its width argument still threads through. The value the
    // field is set from rides alongside as a second signal.
    template <typename Apply>
    void updateBand(PureLayout& layout, Axis axis,
            bq::signal::AnySignal<float> value, Apply apply)
    {
        if (axis == Axis::x)
        {
            layout.width = merge(std::move(layout.width), std::move(value)).map(
                    [apply](Constraints const& c, float v)
                    {
                        Constraints out = c;
                        apply(out, v);
                        return out;
                    });
        }
        else
        {
            auto old = layout.heightForWidth;
            layout.heightForWidth =
                [old, value = std::move(value), apply](
                        bq::signal::AnySignal<float> w)
                    -> bq::signal::AnySignal<Constraints>
                {
                    return merge(old(std::move(w)), value.clone()).map(
                            [apply](Constraints const& c, float v)
                            {
                                Constraints out = c;
                                apply(out, v);
                                return out;
                            });
                };
        }
    }
} // namespace

void addPureConstraint(AnyBuilder& builder, Axis axis,
        bq::signal::AnySignal<LayoutSpec> fragment)
{
    PureLayout layout = pureLayoutOr(builder);

    auto append = [](Constraints const& c, LayoutSpec const& fragment)
    {
        Constraints out = c;
        appendSpec(out.relations, fragment);
        return out;
    };

    if (axis == Axis::x)
    {
        layout.width = merge(std::move(layout.width), std::move(fragment))
            .map(append);
    }
    else
    {
        auto old = layout.heightForWidth;
        layout.heightForWidth =
            [old, fragment = std::move(fragment), append](
                    bq::signal::AnySignal<float> w)
                -> bq::signal::AnySignal<Constraints>
            {
                return merge(old(std::move(w)), fragment.clone()).map(append);
            };
    }

    builder.setPureLayout(std::move(layout));
}

void setPureNatural(AnyBuilder& builder, Axis axis,
        bq::signal::AnySignal<float> value, arrange::Strength strength)
{
    PureLayout layout = pureLayoutOr(builder);
    updateBand(layout, axis, std::move(value),
            [strength](Constraints& c, float v)
            {
                c.natural = BandNatural{ v, strength };
            });
    builder.setPureLayout(std::move(layout));
}

void setPureMin(AnyBuilder& builder, Axis axis,
        bq::signal::AnySignal<float> value)
{
    PureLayout layout = pureLayoutOr(builder);
    updateBand(layout, axis, std::move(value),
            [](Constraints& c, float v) { c.min = v; });
    builder.setPureLayout(std::move(layout));
}

void setPureMax(AnyBuilder& builder, Axis axis,
        bq::signal::AnySignal<float> value)
{
    PureLayout layout = pureLayoutOr(builder);
    updateBand(layout, axis, std::move(value),
            [](Constraints& c, float v) { c.max = v; });
    builder.setPureLayout(std::move(layout));
}

void setPureFlex(AnyBuilder& builder, Axis axis, float coeff)
{
    PureLayout layout = pureLayoutOr(builder);
    updateBand(layout, axis, bq::signal::constant(coeff),
            [](Constraints& c, float v) { c.flex = Flex{ v }; });
    builder.setPureLayout(std::move(layout));
}

void applyPureInset(AnyBuilder& builder, bq::signal::AnySignal<float> inset)
{
    PureLayout layout = pureLayoutOr(builder);
    BoxVariables inner = builder.getBoxVariables();
    BoxVariables outer;

    auto insetShared = std::move(inset).share();

    // The band grows by the inset on both edges and re-tags onto the outer box
    // (which the builder adopts below, so a later flatten emits the grown band
    // there); the old box keeps only the required inner/outer edge relations.
    auto grow = [](Constraints& c, float ins)
    {
        float d = 2.0f * ins;
        if (c.natural)
            c.natural->value += d;
        if (c.min)
            *c.min += d;
        if (c.max)
            *c.max += d;
    };

    layout.width = merge(std::move(layout.width), insetShared.clone()).map(
            [inner, outer, grow](Constraints const& c, float ins)
            {
                Constraints out = c;
                grow(out, ins);
                out.relations.constraints.push_back(
                        arrange::Expression(outer.left)
                            == arrange::Expression(inner.left)
                                - arrange::Expression(static_cast<double>(ins)));
                out.relations.constraints.push_back(
                        arrange::Expression(outer.right)
                            == arrange::Expression(inner.right)
                                + arrange::Expression(static_cast<double>(ins)));
                return out;
            });

    auto oldHeight = layout.heightForWidth;
    layout.heightForWidth =
        [oldHeight, inner, outer, grow, insetShared](
                bq::signal::AnySignal<float> w)
            -> bq::signal::AnySignal<Constraints>
        {
            return merge(oldHeight(std::move(w)), insetShared.clone()).map(
                    [inner, outer, grow](Constraints const& c, float ins)
                    {
                        Constraints out = c;
                        grow(out, ins);
                        out.relations.constraints.push_back(
                                arrange::Expression(outer.top)
                                    == arrange::Expression(inner.top)
                                        - arrange::Expression(
                                            static_cast<double>(ins)));
                        out.relations.constraints.push_back(
                                arrange::Expression(outer.bottom)
                                    == arrange::Expression(inner.bottom)
                                        + arrange::Expression(
                                            static_cast<double>(ins)));
                        return out;
                    });
        };

    builder.setPureLayout(std::move(layout));
    builder.setBoxVariables(outer);
}

LayoutSpec flattenConstraints(Constraints const& constraints,
        BoxVariables const& box, Axis axis)
{
    LayoutSpec spec = constraints.relations;

    auto extent = [&]
    {
        return axis == Axis::x ? box.width() : box.height();
    };

    // A flexing widget's natural is a flex-basis: dropped here so the container's
    // slack distribution (the flex coupling plus the gap drive) is free to
    // stretch it, exactly as a filler carries no natural on its flex axis. It
    // stays in the band for the parent's aggregation; only the stamp onto the box
    // drops it. Without a flex it is baked as the widget's content/natural size.
    bool flexes = constraints.flex && constraints.flex->coeff > 0.0f;
    if (constraints.natural && !flexes)
        spec.constraints.push_back(
                (extent() == arrange::Expression(
                        static_cast<double>(constraints.natural->value)))
                | constraints.natural->strength);
    if (constraints.min)
        spec.constraints.push_back(
                extent() >= arrange::Expression(
                        static_cast<double>(*constraints.min)));
    if (constraints.max)
        spec.constraints.push_back(
                extent() <= arrange::Expression(
                        static_cast<double>(*constraints.max)));

    return spec;
}

arrange::Expression BoxVariables::width() const
{
    return arrange::Expression(right) - arrange::Expression(left);
}

arrange::Expression BoxVariables::height() const
{
    return arrange::Expression(bottom) - arrange::Expression(top);
}

bq::signal::AnySignal<LayoutSolution> solveLayout(
        bq::signal::AnySignal<LayoutSpec> spec)
{
    return std::move(spec).withPrevious(
            [](SolveState state, LayoutSpec const& spec)
            {
                // Each update re-solves from an empty solver rather than
                // editing the previous tableau in place. arrange's incremental
                // remove+add path is not robust to every change of a stable
                // variable set — a pure reorder of a fixed set of boxes can
                // drive its objective unbounded — so the spec, which is rebuilt
                // in full each frame anyway, is applied to a cleared solver.
                // Reusing the tableau across frames is the incremental-solving
                // performance follow-up.
                state.solver.reset();

                try
                {
                    state.solver.setConstraints(spec.constraints);
                }
                catch (arrange::Error const&)
                {
                    // An unsatisfiable update keeps the previous solution rather
                    // than tearing down the whole signal graph.
                    return state;
                }

                LayoutSolution solution;
                solution.reserve(spec.variables.size());
                for (auto const& variable : spec.variables)
                    solution.emplace(variable.id(), state.solver.valueOf(variable));

                state.solution = std::move(solution);
                return state;
            },
            SolveState{})
        .map([](SolveState const& state)
            {
                return state.solution;
            })
        .share();
}

bq::signal::AnySignal<LayoutSolution> layoutRegion(
        bq::signal::AnySignal<std::vector<LayoutSpec>> fragments)
{
    auto spec = std::move(fragments).map(
            [](std::vector<LayoutSpec> const& parts)
            {
                LayoutSpec merged;
                for (auto const& part : parts)
                {
                    merged.constraints.insert(merged.constraints.end(),
                            part.constraints.begin(), part.constraints.end());
                    merged.variables.insert(merged.variables.end(),
                            part.variables.begin(), part.variables.end());
                }
                return merged;
            });

    return solveLayout(bq::signal::AnySignal<LayoutSpec>(std::move(spec)));
}

bq::signal::AnySignal<LayoutSolution> combineSolutions(
        bq::signal::AnySignal<LayoutSolution> horizontal,
        bq::signal::AnySignal<LayoutSolution> vertical)
{
    return merge(std::move(horizontal), std::move(vertical)).map(
            [](LayoutSolution const& horizontal, LayoutSolution const& vertical)
            {
                LayoutSolution merged = horizontal;
                merged.insert(vertical.begin(), vertical.end());
                return merged;
            });
}

avg::Obb readObb(LayoutSolution const& solution, BoxVariables const& box)
{
    float left = valueOf(solution, box.left);
    float top = valueOf(solution, box.top);
    float right = valueOf(solution, box.right);
    float bottom = valueOf(solution, box.bottom);

    return avg::Obb(
            avg::Vector2f(right - left, bottom - top),
            avg::Transform(avg::Vector2f(left, top)));
}

std::vector<arrange::Constraint> anchorConstraints(BoxVariables const& box,
        float left, float top, float right, float bottom)
{
    return {
        arrange::Expression(box.left) == arrange::Expression(left),
        arrange::Expression(box.top) == arrange::Expression(top),
        arrange::Expression(box.right) == arrange::Expression(right),
        arrange::Expression(box.bottom) == arrange::Expression(bottom),
    };
}

arrange::Strength weakestStrength()
{
    // One weak lane holds gravity and natural at weight 1; the default rides the
    // same lane a thousand times lighter, so any of them dominates it and it
    // never ties one to be averaged, yet it is heavy enough to pin an otherwise
    // free axis to a definite value.
    return arrange::Strength::weak(0.001);
}

arrange::Strength contentStrength()
{
    // The heaviest pull in the weak lane, above the fallback default (0.001),
    // the gap drive (0.0008) and the weak(1.0) cross-fill, but still weak — a
    // whole lane below medium/strong/required. So a content leaf settles at its
    // measured size on both axes by default, and stretching is opt-in: only a
    // filler (which drops the natural), a fixed size, a bound or a guide takes it
    // off content. This is the "filler is the flex" model — content by default,
    // fill by opt-in — carried into the strength ladder.
    return arrange::Strength::weak(2.0);
}

arrange::Constraint weakWidthDefault(BoxVariables const& box)
{
    return (box.width() == arrange::Expression(100.0)) | weakestStrength();
}

arrange::Constraint weakHeightDefault(BoxVariables const& box)
{
    return (box.height() == arrange::Expression(100.0)) | weakestStrength();
}

std::vector<arrange::Constraint> boxConstraints(BoxVariables const& container,
        std::vector<BoxVariables> const& children, Axis axis)
{
    std::vector<arrange::Constraint> out;

    // The trailing edge is pulled to the container's end only weakly, with no
    // required cap holding it there. A child free to grow — a filler — settles
    // against that weak pull and fills the container, while content held at a
    // firmer size stays put, leaving the slack as a gap rather than being
    // stretched to fill it. When the content is larger than the container the
    // weak pull simply loses to the children's own sizes, so the last child
    // overflows past the container's end instead of being squeezed to fit.
    auto fill = [&out](arrange::Variable const& childEnd,
            arrange::Variable const& containerEnd)
    {
        out.push_back(
                (arrange::Expression(childEnd) == arrange::Expression(containerEnd))
                | arrange::Strength::weak());
    };

    // Only the main axis is tiled here: consecutive children meet, the first
    // touches the container's leading end, and the last is pulled weakly to its
    // trailing end. The cross axis is left free for placeInSlot() to size and
    // position each child within the container's extent.
    for (std::size_t i = 0; i < children.size(); ++i)
    {
        BoxVariables const& child = children[i];
        bool first = i == 0;
        bool last = i + 1 == children.size();

        if (axis == Axis::y)
        {
            out.push_back(first ? pin(child.top, container.top)
                                : pin(child.top, children[i - 1].bottom));
            if (last)
                fill(child.bottom, container.bottom);
        }
        else
        {
            out.push_back(first ? pin(child.left, container.left)
                                : pin(child.left, children[i - 1].right));
            if (last)
                fill(child.right, container.right);
        }
    }

    return out;
}

void placeInSlot(std::vector<arrange::Constraint>& out,
        arrange::Variable const& contentLead,
        arrange::Variable const& contentTrail,
        arrange::Variable const& slotLead,
        arrange::Variable const& slotTrail,
        float gravity, float maxExtent)
{
    auto contentExtent = [&]
    {
        return arrange::Expression(contentTrail)
            - arrange::Expression(contentLead);
    };
    auto slotExtent = [&]
    {
        return arrange::Expression(slotTrail) - arrange::Expression(slotLead);
    };

    // The content grows to fill the slot but never past its own maximum, so it
    // settles at the smaller of the two. The fill is weak and the cap strong,
    // so the cap wins when it bites.
    out.push_back((contentExtent() == slotExtent()) | arrange::Strength::weak());
    out.push_back(
            (contentExtent() <= arrange::Expression(maxExtent))
            | arrange::Strength::strong());

    // Where the content is smaller than the slot it sits at the gravity fraction
    // of the slack, a weak pull a medium guide alignment overrides. The two weak
    // pulls do not fight: the fill fixes the extent while this fixes the leading
    // offset, independent degrees of freedom.
    out.push_back(
            (arrange::Expression(contentLead) - arrange::Expression(slotLead)
                == static_cast<double>(gravity) * slotExtent()
                    - static_cast<double>(gravity) * contentExtent())
            | arrange::Strength::weak());
}

std::map<avg::UniqueId, arrange::Variable> guideConstraints(
        std::vector<arrange::Constraint>& out,
        std::vector<BoxVariables> const& children,
        std::vector<std::vector<GuideAlignment>> const& alignments,
        ResolvedGuideMap const& resolved)
{
    // One variable per distinct guide id, minted the first time the id is seen
    // and reused for every later alignment to it, so all children sharing a
    // guide meet on the one line. std::map orders by UniqueId, which the id
    // already compares.
    std::map<avg::UniqueId, arrange::Variable> lines;

    auto pinEdge = [&out, &lines](arrange::Expression edge,
            avg::UniqueId const& guide)
    {
        out.push_back(
                (std::move(edge) == arrange::Expression(lines[guide]))
                | arrange::Strength::medium());
    };

    for (std::size_t i = 0; i < children.size() && i < alignments.size(); ++i)
    {
        BoxVariables const& child = children[i];

        for (GuideAlignment const& alignment : alignments[i])
        {
            switch (alignment.edge)
            {
            case GuideEdge::left:
                pinEdge(arrange::Expression(child.left), alignment.guide);
                break;
            case GuideEdge::right:
                pinEdge(arrange::Expression(child.right), alignment.guide);
                break;
            case GuideEdge::centerX:
                pinEdge(arrange::Expression(child.left)
                        + arrange::Expression(child.right)
                        - arrange::Expression(lines[alignment.guide]),
                        alignment.guide);
                break;
            case GuideEdge::top:
                pinEdge(arrange::Expression(child.top), alignment.guide);
                break;
            case GuideEdge::bottom:
                pinEdge(arrange::Expression(child.bottom), alignment.guide);
                break;
            case GuideEdge::centerY:
                pinEdge(arrange::Expression(child.top)
                        + arrange::Expression(child.bottom)
                        - arrange::Expression(lines[alignment.guide]),
                        alignment.guide);
                break;
            }
        }
    }

    // The line carries no size or position of its own — only the medium pulls of
    // the edges it gathers reach it — so on its own it is a free variable the
    // solver may leave undefined, pivoting the aligned edges to that undefined
    // value differently by variable-id order. Every line is therefore anchored to
    // a definite position, and each line gets exactly one anchor so no two
    // competing soft pulls on the same variable can pivot apart by platform.
    //
    // A line an ancestor firewall already resolved is fixed to that inherited
    // constant by a required equality: an absolute constraint the solver enforces
    // structurally rather than as a soft error term, so no pivot order can move it
    // and the aligned edges follow it across the boundary. The line is coupled to
    // the edges only at medium, so this required pin can never make the solve
    // infeasible. A resolved guide no child here names is simply absent from
    // lines and pins nothing.
    //
    // A line no ancestor resolved is genuinely free, so it gets a weak stay at
    // zero. The stay is weaker than an edge's gravity default, so it never
    // displaces where the edges settle; it only breaks the pivot's freedom,
    // resolving the line to the position its edges already agree on.
    for (auto const& line : lines)
    {
        auto it = resolved.find(line.first);
        if (it != resolved.end())
            out.push_back(
                    arrange::Expression(line.second)
                        == arrange::Expression(it->second));
        else
            out.push_back(
                    (arrange::Expression(line.second) == arrange::Expression(0.0))
                    | arrange::Strength::weak(0.01));
    }

    return lines;
}

std::vector<arrange::Constraint> guideConstraints(
        std::vector<BoxVariables> const& children,
        std::vector<std::vector<GuideAlignment>> const& alignments)
{
    std::vector<arrange::Constraint> out;
    guideConstraints(out, children, alignments, ResolvedGuideMap());
    return out;
}

GridLines gridLines(std::vector<arrange::Constraint>& out,
        BoxVariables const& container, unsigned int columns, unsigned int rows)
{
    // The grid lines: columns + 1 vertical lines left to right, rows + 1
    // horizontal lines. The horizontal lines are indexed bottom to top, so
    // ys[0] is the container's bottom edge in the solver's top-down space and
    // ys[rows] its top. Both families span the container and are pinned to
    // equal intervals, which is what makes every column the same width and
    // every row the same height.
    GridLines lines;
    lines.xs.resize(columns + 1);
    lines.ys.resize(rows + 1);

    auto equalIntervals = [&out](std::vector<arrange::Variable> const& lines)
    {
        for (std::size_t i = 1; i + 1 < lines.size(); ++i)
            out.push_back(
                    (arrange::Expression(lines[i + 1])
                        - arrange::Expression(lines[i]))
                    == (arrange::Expression(lines[i])
                        - arrange::Expression(lines[i - 1])));
    };

    out.push_back(pin(lines.xs.front(), container.left));
    out.push_back(pin(lines.xs.back(), container.right));
    equalIntervals(lines.xs);

    out.push_back(pin(lines.ys.front(), container.bottom));
    out.push_back(pin(lines.ys.back(), container.top));
    equalIntervals(lines.ys);

    return lines;
}

} // namespace bqui::widget
