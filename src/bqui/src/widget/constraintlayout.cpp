#include "constraintlayout.h"

#include "bqui/widget/builder.h"


#include <avg/transform.h>
#include <avg/vector.h>

#include <arrange/errors.h>
#include <arrange/solver.h>
#include <arrange/strength.h>

#include <bq/signal/constant.h>
#include <bq/signal/merge.h>
#include <bq/signal/signal.h>

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
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
        std::string unmet;
    };

    void logLayout(std::string const& message)
    {
        std::cerr << "bqui layout: " << message << std::endl;
    }

    char const* strengthName(arrange::Strength const& strength)
    {
        if (strength == regionAnchorStrength())
            return "region";
        if (strength == minStrength())
            return "min";
        if (strength == maxStrength())
            return "max";
        if (strength == fixedStrength())
            return "fixed";
        if (strength == alignStrength())
            return "align";
        return "strong";
    }

    // Lists the strong constraints the solution leaves violated, by kind and
    // with the solved left-hand side against the stated value. Required ones
    // hold by construction and the weak lane is violated by design, so only
    // the strong lane is checked: one pass over its expressions per solve.
    std::string describeUnmet(arrange::Solver const& solver,
            std::vector<arrange::Constraint> const& constraints)
    {
        double const tolerance = 0.01;
        std::ostringstream out;
        std::size_t count = 0;
        for (auto const& c : constraints)
        {
            arrange::Strength strength = c.strength();
            if (strength.isRequired() || strength < arrange::Strength::strong())
                continue;

            arrange::Expression const& e = c.expression();
            double lhs = 0.0;
            for (std::size_t i = 0; i < e.termCount(); ++i)
            {
                auto term = e.term(i);
                lhs += term.coefficient * solver.valueOf(term.variable);
            }
            double rhs = 0.0 - e.constant();
            double error = lhs - rhs;

            char const* op = "==";
            bool met = std::abs(error) <= tolerance;
            if (c.relation() == arrange::Relation::le)
            {
                op = "<=";
                met = error <= tolerance;
            }
            else if (c.relation() == arrange::Relation::ge)
            {
                op = ">=";
                met = error >= -tolerance;
            }
            if (met)
                continue;

            out << (count++ ? ", " : "") << strengthName(strength) << " ("
                << lhs << " " << op << " " << rhs << ")";
        }
        if (!count)
            return {};
        return std::to_string(count) + " strong constraint(s) unmet: "
            + out.str();
    }

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
    // Maps the axis's Constraints band through @p apply, threading the value the
    // field is set from alongside as a second signal. Reads and rebuilds through
    // the PureLayout interface so the untouched axis passes through unchanged.
    template <typename Apply>
    void updateBand(PureLayout& layout, Axis axis,
            bq::signal::AnySignal<float> value, Apply apply)
    {
        PureLayout old = layout;

        if (axis == Axis::x)
        {
            auto width = merge(old.getWidth(), std::move(value)).map(
                    [apply](Constraints const& c, float v)
                    {
                        Constraints out = c;
                        apply(out, v);
                        return out;
                    });
            layout = simplePureLayout(
                    bq::signal::AnySignal<Constraints>(std::move(width)),
                    [old](bq::signal::AnySignal<LayoutSolution> ws)
                    {
                        return old.getHeightForWidth(std::move(ws));
                    });
        }
        else
        {
            auto shared = std::move(value).share();
            layout = simplePureLayout(
                    old.getWidth(),
                    [old, shared, apply](
                            bq::signal::AnySignal<LayoutSolution> ws)
                        -> bq::signal::AnySignal<Constraints>
                    {
                        return merge(old.getHeightForWidth(std::move(ws)),
                                shared.clone()).map(
                                [apply](Constraints const& c, float v)
                                {
                                    Constraints out = c;
                                    apply(out, v);
                                    return out;
                                });
                    });
        }
    }
} // namespace

void setPureNatural(AnyBuilder& builder, Axis axis,
        bq::signal::AnySignal<float> value, arrange::Strength strength)
{
    PureLayout layout = builder.getPureLayout();
    updateBand(layout, axis, std::move(value),
            [strength](Constraints& c, float v)
            {
                c.natural = BandNatural{ v, strength };
            });
    builder.setPureLayout(std::move(layout));
}

void setPureFixed(AnyBuilder& builder, Axis axis,
        bq::signal::AnySignal<float> value)
{
    PureLayout layout = builder.getPureLayout();
    updateBand(layout, axis, std::move(value),
            [](Constraints& c, float v)
            {
                c.natural = BandNatural{ v, fixedStrength() };
                c.flex.reset();
            });
    builder.setPureLayout(std::move(layout));
}

void setPureFlex(AnyBuilder& builder, Axis axis,
        bq::signal::AnySignal<float> weight)
{
    PureLayout layout = builder.getPureLayout();
    updateBand(layout, axis, std::move(weight),
            [](Constraints& c, float w) { c.flex = Flex{ w }; });
    builder.setPureLayout(std::move(layout));
}

void setPureMin(AnyBuilder& builder, Axis axis,
        bq::signal::AnySignal<float> value)
{
    PureLayout layout = builder.getPureLayout();
    updateBand(layout, axis, std::move(value),
            [](Constraints& c, float v) { c.min = v; });
    builder.setPureLayout(std::move(layout));
}

void setPureMax(AnyBuilder& builder, Axis axis,
        bq::signal::AnySignal<float> value)
{
    PureLayout layout = builder.getPureLayout();
    updateBand(layout, axis, std::move(value),
            [](Constraints& c, float v) { c.max = v; });
    builder.setPureLayout(std::move(layout));
}

void setPureAnchor(AnyBuilder& builder, Axis axis, std::string name,
        bq::signal::AnySignal<Anchor> anchor)
{
    PureLayout old = builder.getPureLayout();
    auto shared = std::move(anchor).share();
    auto apply = [name = std::move(name)](Constraints const& c, Anchor a)
    {
        Constraints out = c;
        out.anchors[name] = a;
        return out;
    };

    if (axis == Axis::x)
    {
        builder.setPureLayout(simplePureLayout(
                bq::signal::AnySignal<Constraints>(
                    merge(old.getWidth(), shared.clone()).map(apply)),
                [old](bq::signal::AnySignal<LayoutSolution> ws)
                {
                    return old.getHeightForWidth(std::move(ws));
                }));
    }
    else
    {
        builder.setPureLayout(simplePureLayout(old.getWidth(),
                [old, shared, apply](
                        bq::signal::AnySignal<LayoutSolution> ws)
                    -> bq::signal::AnySignal<Constraints>
                {
                    return merge(old.getHeightForWidth(std::move(ws)),
                            shared.clone()).map(apply);
                }));
    }
}

void applyPureInset(AnyBuilder& builder, bq::signal::AnySignal<float> inset)
{
    PureLayout layout = builder.getPureLayout();
    BoxVariables inner = builder.getBoxVariables();
    BoxVariables outer;

    auto insetShared = std::move(inset).share();

    // The band grows by the inset on both edges and re-tags onto the outer box
    // (which the builder adopts below, so a later flatten emits the grown band
    // there); the old box keeps only the required inner/outer edge relations.
    // An anchor at inner + f * (outer extent - 2 * inset) + o is re-expressed
    // against the outer box, whose leading edge sits one inset further out.
    auto grow = [](Constraints& c, float ins)
    {
        float d = 2.0f * ins;
        if (c.natural)
            c.natural->value += d;
        if (c.min)
            *c.min += d;
        if (c.max)
            *c.max += d;
        for (auto& [name, anchor] : c.anchors)
            anchor.offset += ins * (1.0f - 2.0f * anchor.fraction);
    };

    PureLayout old = layout;

    auto width = merge(old.getWidth(), insetShared.clone()).map(
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

    layout = simplePureLayout(
        bq::signal::AnySignal<Constraints>(std::move(width)),
        [old, inner, outer, grow, insetShared](
                bq::signal::AnySignal<LayoutSolution> ws)
            -> bq::signal::AnySignal<Constraints>
        {
            return merge(old.getHeightForWidth(std::move(ws)),
                    insetShared.clone()).map(
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
        });

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
    {
        spec.constraints.push_back(
                (extent() == arrange::Expression(
                        static_cast<double>(constraints.natural->value)))
                | constraints.natural->strength);
    }
    if (constraints.min)
    {
        spec.constraints.push_back(
                (extent() >= arrange::Expression(
                        static_cast<double>(*constraints.min)))
                | minStrength());
    }
    if (constraints.max)
    {
        spec.constraints.push_back(
                (extent() <= arrange::Expression(
                        static_cast<double>(*constraints.max)))
                | maxStrength());
    }

    // An over-full container hands a flexing child a negative share of the
    // slack; the extent stops at zero and the deficit overflows instead.
    spec.constraints.push_back(
            (extent() >= arrange::Expression(0.0)) | minStrength());

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
                catch (arrange::Error const& e)
                {
                    // Only the structural relations are required, and they are
                    // always jointly satisfiable, so this means a bug in a
                    // fragment. Keeping the previous solution leaves a first
                    // solve's region at zero size.
                    logLayout(std::string("infeasible solve, keeping the "
                                "previous solution: ") + e.what());
                    return state;
                }

                std::string unmet = describeUnmet(state.solver,
                        spec.constraints);
                if (!unmet.empty() && unmet != state.unmet)
                    logLayout(unmet);
                state.unmet = std::move(unmet);

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
        (arrange::Expression(box.left) == arrange::Expression(left))
            | fixedStrength(),
        (arrange::Expression(box.top) == arrange::Expression(top))
            | fixedStrength(),
        (arrange::Expression(box.right) == arrange::Expression(right))
            | fixedStrength(),
        (arrange::Expression(box.bottom) == arrange::Expression(bottom))
            | fixedStrength(),
    };
}

// The stated sizes rank min > max > fixed (as CSS ranks min-width over
// max-width over width) by scaling the strong lane, with the region anchor
// above them all. The weights stay small: the objective mixes them with the
// weak(1e-5) flex coupling, which a much heavier strong weight would lose to
// round-off.
arrange::Strength regionAnchorStrength()
{
    return arrange::Strength::strong(10.0);
}

arrange::Strength minStrength()
{
    return arrange::Strength::strong(3.0);
}

arrange::Strength maxStrength()
{
    return arrange::Strength::strong(2.0);
}

arrange::Strength fixedStrength()
{
    return arrange::Strength::strong(1.0);
}

arrange::Strength alignStrength()
{
    return arrange::Strength::strong(1.5);
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
    // Top of the weak lane: above the fallback default (0.001), the gap drive
    // (0.0008) and the weak(1.0) cross-fill, but a whole lane below medium.
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

void placeInSlot(std::vector<arrange::Constraint>& out,
        arrange::Variable const& contentLead,
        arrange::Variable const& contentTrail,
        arrange::Variable const& slotLead,
        arrange::Variable const& slotTrail,
        float gravity, float maxExtent,
        arrange::Strength fillStrength)
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
    out.push_back((contentExtent() == slotExtent()) | fillStrength);
    out.push_back(
            (contentExtent() <= arrange::Expression(maxExtent))
            | maxStrength());

    // The fill fixes the extent while the gravity pull fixes the leading
    // offset, independent degrees of freedom, so the two weak pulls do not
    // fight.
    placeAtGravity(out, contentLead, contentTrail, slotLead, slotTrail,
            gravity);
}

void placeAtGravity(std::vector<arrange::Constraint>& out,
        arrange::Variable const& contentLead,
        arrange::Variable const& contentTrail,
        arrange::Variable const& slotLead,
        arrange::Variable const& slotTrail,
        float gravity)
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

    // A weak pull any medium constraint overrides.
    out.push_back(
            (arrange::Expression(contentLead) - arrange::Expression(slotLead)
                == static_cast<double>(gravity) * slotExtent()
                    - static_cast<double>(gravity) * contentExtent())
            | arrange::Strength::weak());
}

void gridAxisConstraints(std::vector<arrange::Constraint>& out,
        std::vector<arrange::Variable> const& lines,
        arrange::Variable const& spanLead, arrange::Variable const& spanTrail)
{
    out.push_back(pin(lines.front(), spanLead));
    out.push_back(pin(lines.back(), spanTrail));

    for (std::size_t i = 1; i + 1 < lines.size(); ++i)
        out.push_back(
                (arrange::Expression(lines[i + 1])
                    - arrange::Expression(lines[i]))
                == (arrange::Expression(lines[i])
                    - arrange::Expression(lines[i - 1])));
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

    gridAxisConstraints(out, lines.xs, container.left, container.right);
    gridAxisConstraints(out, lines.ys, container.bottom, container.top);

    return lines;
}

} // namespace bqui::widget
