#include <bqui/modifier/handlegravity.h>
#include <bqui/modifier/instancemodifier.h>
#include "modifier/setsizehint.h"
#include <bqui/modifier/widgetmodifier.h>

#include <bqui/widget/box.h>
#include <bqui/widget/guide.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/resolvedguides.h>
#include <bqui/widget/stack.h>
#include <bqui/widget/uniformgrid.h>
#include <bqui/widget/vbox.h>
#include <bqui/widget/widget.h>

#include <bqui/modifier/alignguide.h>
#include <bqui/modifier/setparams.h>

#include "widget/constraintbox.h"
#include "widget/guideaccess.h"

#include <bqui/buildparams.h>
#include <bqui/inputarea.h>
#include <bqui/mapsizehint.h>
#include <bqui/simplesizehint.h>
#include <bqui/sizehint.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/collection.h>
#include <bq/signal/collectionsignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/input.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <avg/obb.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <btl/uniqueid.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace bqui;
using namespace bqui::widget;

namespace
{

/**
 * @brief Position and size a layout gave to one child.
 *
 * The position is in the coordinates of the layout that was realised, with
 * the origin at its bottom-left corner.
 */
struct Geometry
{
    avg::Vector2f position;
    avg::Vector2f size;
};

/**
 * @brief Creates probe widgets and reads back the geometry they were given.
 *
 * A probe is an empty widget with a fixed size hint that tags its realised
 * Instance with an InputArea carrying a unique id. Input areas are the one
 * piece of geometry that travels intact from a leaf to the root Instance:
 * every enclosing transform is accumulated into the area's own transform
 * while its obb stays in the leaf's local coordinates. Realising the layout
 * and looking up the areas by id therefore recovers both the size each child
 * was allocated and where it was placed, without rendering anything.
 */
class ProbeSet
{
public:
    ProbeSet();

    /**
     * @brief Registers a probe and builds the widget that carries it.
     */
    AnyWidget add(Band width, Band height);

    /**
     * @brief Registers a probe that reports a first baseline on its vertical
     * axis, for baseline-alignment tests.
     */
    AnyWidget add(Band width, Band height, float baseline);

    /**
     * @brief Registers a probe and returns its index.
     *
     * The index is what a dynamic child list carries, so that the widget can
     * be built from the index signal a forEach delegate is handed rather than
     * ahead of time.
     */
    size_t addIndexed(Band width, Band height,
            std::optional<float> baseline = std::nullopt);

    /**
     * @brief Builds the widget for whichever probe @p index names.
     *
     * Everything the widget needs is read out of the signal, so one call
     * covers a probe that is known when the tree is described and one that a
     * forEach delegate is asked for later.
     */
    AnyWidget fromSignal(bq::signal::AnySignal<size_t> index) const;

    /**
     * @brief Realises @p widget at @p size and reads every probe back.
     */
    std::vector<std::optional<Geometry>> realise(AnyWidget widget,
            avg::Vector2f size) const;

    /**
     * @brief Reads every probe out of an already realised instance.
     *
     * Entries come in the order the probes were added and are empty for a
     * probe the instance does not contain. A test that drives updates owns
     * the SignalContext itself and calls this after each pass.
     */
    std::vector<std::optional<Geometry>> read(Instance const& instance) const;

private:
    struct Probe
    {
        btl::UniqueId id;
        Band width;
        Band height;
        std::optional<float> baseline;
    };

    // Held behind a pointer so that a copy taken by a delegate still sees a
    // probe the test registers after the tree was described.
    std::shared_ptr<std::vector<Probe>> probes_;
};

ProbeSet::ProbeSet() :
    probes_(std::make_shared<std::vector<Probe>>())
{
}

AnyWidget ProbeSet::add(Band width, Band height)
{
    return fromSignal(bq::signal::constant(addIndexed(width, height)));
}

AnyWidget ProbeSet::add(Band width, Band height, float baseline)
{
    return fromSignal(bq::signal::constant(
                addIndexed(width, height, baseline)));
}

size_t ProbeSet::addIndexed(Band width, Band height,
        std::optional<float> baseline)
{
    probes_->push_back(Probe{ btl::makeUniqueId(), width, height, baseline });

    return probes_->size() - 1;
}

AnyWidget ProbeSet::fromSignal(bq::signal::AnySignal<size_t> index) const
{
    auto probes = probes_;

    auto id = index.map([probes](size_t i)
            {
                return probes->at(i).id;
            });

    auto hint = index.map([probes](size_t i) -> SizeHint
            {
                Probe const& probe = probes->at(i);

                SizeHint hint = simpleSizeHint(probe.width, probe.height);
                if (!probe.baseline)
                    return hint;

                float baseline = *probe.baseline;
                return mapSizeHint(std::move(hint),
                        [](AxisHint h)
                        {
                            return h;
                        },
                        [baseline](AxisHint h, float)
                        {
                            h.anchors.firstBaseline = baseline;
                            return h;
                        },
                        [](AxisHint h, float)
                        {
                            return h;
                        });
            });

    return makeWidget()
        | modifier::makeWidgetModifier(modifier::makeInstanceModifier(
                    [](Instance instance, btl::UniqueId id)
                    {
                        auto areas = instance.getInputAreas();
                        areas.push_back(makeInputArea(id, instance.getObb()));

                        return std::move(instance)
                            .setInputAreas(std::move(areas));
                    }, std::move(id)))
        | modifier::setSizeHint(std::move(hint))
        ;
}

std::vector<std::optional<Geometry>> ProbeSet::realise(AnyWidget widget,
        avg::Vector2f size) const
{
    auto instanceSignal = std::move(widget)(BuildParams())(
            bq::signal::constant(size))
        .getInstance();

    // The whole tree is evaluated through this one context. A signal evaluated
    // in a context of its own would be a different, parallel instantiation of
    // the same graph (bq::signal::SignalContext).
    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    return read(context.evaluate<0>().get<0>());
}

std::vector<std::optional<Geometry>> ProbeSet::read(
        Instance const& instance) const
{
    std::vector<std::optional<Geometry>> result;
    result.reserve(probes_->size());

    for (auto const& probe : *probes_)
    {
        InputArea const* found = nullptr;
        for (auto const& area : instance.getInputAreas())
            if (area.getId() == probe.id)
                found = &area;

        if (!found)
        {
            result.push_back(std::nullopt);
            continue;
        }

        result.push_back(Geometry{
                found->getTransform().getTranslation(),
                found->getObbs().front().getSize()
                });
    }

    return result;
}

void expectGeometry(std::string const& label,
        std::optional<Geometry> const& geometry,
        float x, float y, float width, float height)
{
    SCOPED_TRACE(label);

    ASSERT_TRUE(geometry.has_value()) << "probe was not realised";

    EXPECT_FLOAT_EQ(x, geometry->position[0]);
    EXPECT_FLOAT_EQ(y, geometry->position[1]);
    EXPECT_FLOAT_EQ(width, geometry->size[0]);
    EXPECT_FLOAT_EQ(height, geometry->size[1]);
}

void expectNotRealised(std::string const& label,
        std::optional<Geometry> const& geometry)
{
    SCOPED_TRACE(label);

    EXPECT_FALSE(geometry.has_value());
}

Band const fixed50 = { 50.0f, 50.0f, 50.0f };

// Three hints that leave the minimums and the naturals satisfiable at a total
// of 150, so only part of the filler range is handed out and the layout has to
// distribute it: minimums total 60, naturals 100. Only stretchyHint grows, so
// it alone absorbs the surplus its neighbours leave.
Band const smallHint = { 20.0f, 40.0f, 40.0f };
Band const stretchyHint = { 10.0f, 30.0f, 130.0f, 1.0f };
Band const rigidHint = { 30.0f, 30.0f, 30.0f };

// A hint whose minimum and natural are both zero, that grows, and whose max is
// far larger than any container used here, so a probe carrying it takes
// whatever it is offered on that axis.
Band const fillHint = { 0.0f, 0.0f, 1000.0f, 1.0f };

Band const fixed100 = { 100.0f, 100.0f, 100.0f };
Band const fixed150 = { 150.0f, 150.0f, 150.0f };

using Children = std::vector<AnyWidget>;

/**
 * @brief Counts how many times @p widget is built.
 */
AnyWidget countBuilds(std::shared_ptr<int> builds, AnyWidget widget)
{
    return makeWidget([builds, widget]() -> AnyWidget
            {
                ++*builds;

                return widget;
            });
}

/**
 * @brief A child list whose membership the test drives.
 *
 * Each probe is keyed by its own index, so a probe keeps its identity across
 * additions, removals and reorderings.
 */
bq::signal::ArraySignal<AnyWidget> dynamicChildren(ProbeSet probes,
        bq::signal::AnySignal<std::vector<size_t>> indices,
        std::shared_ptr<int> builds = nullptr)
{
    return bq::signal::forEach(std::move(indices),
            [](size_t index)
            {
                return index;
            },
            [probes, builds](bq::signal::AnySignal<size_t> index)
            {
                AnyWidget widget = probes.fromSignal(std::move(index));

                if (!builds)
                    return widget;

                return countBuilds(builds, std::move(widget));
            });
}

/**
 * @brief A child list built from a collection of probe indices.
 *
 * Each item names the probe at its value plus @p offset, so two lists over one
 * collection can still carry probes of their own.
 */
bq::signal::ArraySignal<AnyWidget> collectionChildren(ProbeSet probes,
        bq::signal::Collection<size_t> const& indices,
        std::shared_ptr<int> builds, size_t offset = 0)
{
    return bq::signal::forEach(indices,
            [probes, builds, offset](bq::signal::AnySignal<size_t> index)
            {
                auto probe = bq::signal::AnySignal<size_t>(index.map(
                            [offset](size_t i)
                            {
                                return i + offset;
                            }));

                return countBuilds(builds, probes.fromSignal(std::move(probe)));
            });
}

bq::signal::Collection<size_t> makeIndices(std::vector<size_t> indices)
{
    bq::signal::Collection<size_t> collection;
    auto transaction = collection.write();
    for (size_t index : indices)
        transaction.pushBack(index);

    return collection;
}

bq::signal::FrameInfo nextFrame(uint64_t frameId)
{
    return bq::signal::FrameInfo(frameId, std::chrono::microseconds(0));
}

/**
 * @brief A size hint that records the size it was last queried at.
 *
 * Its height for a width and its width for a height are the size queried.
 */
struct QueryRecordingHint
{
    AxisHint getWidth() const
    {
        return AxisHint{ fillHint, Anchors{} };
    }

    AxisHint getHeightForWidth(float width) const
    {
        *queried = width;
        return AxisHint{ Band{ width, width, width }, Anchors{} };
    }

    AxisHint getWidthForHeight(float height) const
    {
        *queried = height;
        return AxisHint{ Band{ height, height, height }, Anchors{} };
    }

    std::shared_ptr<float> queried;
};

} // anonymous namespace

TEST(Layout, hboxDistributesFillerSpace)
{
    ProbeSet probes;

    Children children;
    children.push_back(probes.add(smallHint, fixed50));
    children.push_back(probes.add(stretchyHint, fixed50));
    children.push_back(probes.add(rigidHint, fixed50));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(150.0f, 50.0f));

    ASSERT_EQ(3u, geometries.size());

    // Naturals cost 100 of the 150, so half of each child's filler range is
    // granted: 40, 30 + 50 and 30.
    expectGeometry("small", geometries[0], 0.0f, 0.0f, 40.0f, 50.0f);
    expectGeometry("stretchy", geometries[1], 40.0f, 0.0f, 80.0f, 50.0f);
    expectGeometry("rigid", geometries[2], 120.0f, 0.0f, 30.0f, 50.0f);
}

TEST(Layout, hboxGrantsFullFillerWhenSpaceAllows)
{
    ProbeSet probes;

    Children children;
    children.push_back(probes.add(smallHint, fixed50));
    children.push_back(probes.add(stretchyHint, fixed50));
    children.push_back(probes.add(rigidHint, fixed50));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(200.0f, 50.0f));

    ASSERT_EQ(3u, geometries.size());

    expectGeometry("small", geometries[0], 0.0f, 0.0f, 40.0f, 50.0f);
    expectGeometry("stretchy", geometries[1], 40.0f, 0.0f, 130.0f, 50.0f);
    expectGeometry("rigid", geometries[2], 170.0f, 0.0f, 30.0f, 50.0f);
}

TEST(Layout, vboxStacksFromTopDown)
{
    ProbeSet probes;

    Children children;
    children.push_back(probes.add(fixed50, smallHint));
    children.push_back(probes.add(fixed50, stretchyHint));
    children.push_back(probes.add(fixed50, rigidHint));

    auto geometries = probes.realise(vbox(std::move(children)),
            avg::Vector2f(50.0f, 150.0f));

    ASSERT_EQ(3u, geometries.size());

    // The y axis points up, so the first child occupies the topmost band and
    // the last one sits at the origin.
    expectGeometry("small", geometries[0], 0.0f, 110.0f, 50.0f, 40.0f);
    expectGeometry("stretchy", geometries[1], 0.0f, 30.0f, 50.0f, 80.0f);
    expectGeometry("rigid", geometries[2], 0.0f, 0.0f, 50.0f, 30.0f);
}

// A column laid out through the arrange solver rather than the SizeHint
// arithmetic. Three rigid children whose heights fill the container exactly
// leave the solve fully determined, so each one lands where the edge-to-edge
// stack puts it: the first at the top, the last at the origin, every child
// spanning the container's width. This proves the solver drives real widget
// geometry end to end (box variables collected from the builders, constraints
// emitted, one Solver folded over the spec, solved boxes flipped back into the
// y-up widget tree).
TEST(Layout, solverVboxStacksChildrenThroughTheSolver)
{
    ProbeSet probes;

    Band const width = { 50.0f, 50.0f, 50.0f };

    std::vector<AnyWidget> children;
    children.push_back(probes.add(width, Band{ 40.0f, 40.0f, 40.0f }));
    children.push_back(probes.add(width, Band{ 30.0f, 30.0f, 30.0f }));
    children.push_back(probes.add(width, Band{ 50.0f, 50.0f, 50.0f }));

    auto geometries = probes.realise(solverVbox(std::move(children)),
            avg::Vector2f(50.0f, 120.0f));

    ASSERT_EQ(3u, geometries.size());

    // Solver window space stacks 0..40, 40..70, 70..120 from the top; the y-up
    // flip turns each child's bottom edge into its origin.
    expectGeometry("first (top)", geometries[0], 0.0f, 80.0f, 50.0f, 40.0f);
    expectGeometry("middle", geometries[1], 0.0f, 50.0f, 50.0f, 30.0f);
    expectGeometry("last (origin)", geometries[2], 0.0f, 0.0f, 50.0f, 50.0f);
}

// The regression case the exact-fill pin got wrong. Three rigid children total
// 120 in a 150-tall container with no filler among them, so the column packs
// them against the top and leaves the spare 30 as a gap at the bottom. A
// required exact-fill pin would instead stretch the children to fill the
// container and gravity would then centre each in its stretched slot, spreading
// them out — the weak fill pin keeps the medium natural sizes winning, so the
// stack packs at the top.
TEST(Layout, solverVboxPacksContentShorterThanContainerAtTheTop)
{
    Band const width = { 50.0f, 50.0f, 50.0f };
    Band const a = { 40.0f, 40.0f, 40.0f };
    Band const b = { 30.0f, 30.0f, 30.0f };
    Band const c = { 50.0f, 50.0f, 50.0f };

    avg::Vector2f const size(50.0f, 150.0f);

    ProbeSet solverProbes;
    std::vector<AnyWidget> children;
    children.push_back(solverProbes.add(width, a));
    children.push_back(solverProbes.add(width, b));
    children.push_back(solverProbes.add(width, c));
    auto solved = solverProbes.realise(solverVbox(std::move(children)), size);

    ASSERT_EQ(3u, solved.size());

    // Each rigid child at its natural height, stacked from the top with the
    // spare 30 left as a gap at the bottom.
    expectGeometry("first (top)", solved[0], 0.0f, 110.0f, 50.0f, 40.0f);
    expectGeometry("middle", solved[1], 0.0f, 80.0f, 50.0f, 30.0f);
    expectGeometry("last", solved[2], 0.0f, 30.0f, 50.0f, 50.0f);
}

// Content taller than the container, no filler. With the required containment
// cap dropped, an over-full box overflows instead of squashing: each child
// keeps its natural height and the last extends past the container's end rather
// than being compressed to fit.
TEST(Layout, solverVboxOverflowsOverfullContent)
{
    Band const width = { 50.0f, 50.0f, 50.0f };
    Band const content = { 20.0f, 50.0f, 50.0f };

    avg::Vector2f const size(50.0f, 80.0f);

    ProbeSet solverProbes;
    std::vector<AnyWidget> children;
    children.push_back(solverProbes.add(width, content));
    children.push_back(solverProbes.add(width, content));
    auto solved = solverProbes.realise(solverVbox(std::move(children)), size);

    ASSERT_EQ(2u, solved.size());

    // Both children keep their natural 50. The first sits at the top; the second
    // stacks below it and overflows past the container's bottom, so its origin
    // lands 20 below the container in the y-up widget space.
    expectGeometry("first", solved[0], 0.0f, 30.0f, 50.0f, 50.0f);
    expectGeometry("second", solved[1], 0.0f, -20.0f, 50.0f, 50.0f);
}

// Two fillers with a rigid child above the leftover space. Both fillers are
// tied to the container's single stretch variable, so the weak fill splits the
// 100 of leftover space equally: 50 each.
TEST(Layout, solverVboxSplitsLeftoverBetweenFillers)
{
    Band const width = { 50.0f, 50.0f, 50.0f };
    Band const rigid = { 40.0f, 40.0f, 40.0f };

    avg::Vector2f const size(50.0f, 140.0f);

    ProbeSet solverProbes;
    std::vector<AnyWidget> children;
    children.push_back(solverProbes.add(width, rigid));
    children.push_back(solverProbes.add(width, fillHint));
    children.push_back(solverProbes.add(width, fillHint));
    auto solved = solverProbes.realise(solverVbox(std::move(children)), size);

    ASSERT_EQ(3u, solved.size());

    // Rigid child keeps its 40 at the top; the two fillers split the remaining
    // 100 into 50 each.
    expectGeometry("rigid (top)", solved[0], 0.0f, 100.0f, 50.0f, 40.0f);
    expectGeometry("filler one", solved[1], 0.0f, 50.0f, 50.0f, 50.0f);
    expectGeometry("filler two", solved[2], 0.0f, 0.0f, 50.0f, 50.0f);
}

// A single filler under the weak fill pin grows at no cost to the medium size
// pins, so it takes exactly the space the rigid children leave. A 40 rigid
// child above a filler in a 150-tall box leaves the filler 110.
TEST(Layout, solverVboxFillsASingleFiller)
{
    ProbeSet probes;

    Band const width = { 50.0f, 50.0f, 50.0f };

    std::vector<AnyWidget> children;
    children.push_back(probes.add(width, Band{ 40.0f, 40.0f, 40.0f }));
    children.push_back(probes.add(width, fillHint));

    auto geometries = probes.realise(solverVbox(std::move(children)),
            avg::Vector2f(50.0f, 150.0f));

    ASSERT_EQ(2u, geometries.size());

    expectGeometry("rigid (top)", geometries[0], 0.0f, 110.0f, 50.0f, 40.0f);
    expectGeometry("filler", geometries[1], 0.0f, 0.0f, 50.0f, 110.0f);
}

TEST(Layout, gravityCentersAChildInsideItsSlot)
{
    ProbeSet probes;

    Children children;
    children.push_back(probes.add(
                Band{ 20.0f, 50.0f, 50.0f },
                Band{ 10.0f, 30.0f, 30.0f }
                ));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(200.0f, 100.0f));

    ASSERT_EQ(1u, geometries.size());

    // The child cannot use more than 50x30, and the default gravity is
    // (0.5, 0.5), so it is centered in the 50x100 slot the hbox gave it.
    expectGeometry("centered", geometries[0], 0.0f, 35.0f, 50.0f, 30.0f);
}

// The cross-axis twin of gravityCentersAChildInsideItsSlot for a column, pinning
// the placement now folded into the solve rather than run as a post-pass. A
// child narrower than the column keeps its natural width and, under the default
// (0.5, 0.5) gravity, centres across the column's width.
TEST(Layout, gravityCentersAChildAcrossAColumn)
{
    ProbeSet probes;

    std::vector<AnyWidget> children;
    children.push_back(probes.add(
                Band{ 40.0f, 40.0f, 40.0f },
                Band{ 30.0f, 30.0f, 30.0f }
                ));

    auto geometries = probes.realise(solverVbox(std::move(children)),
            avg::Vector2f(100.0f, 60.0f));

    ASSERT_EQ(1u, geometries.size());

    // The child wants only 40 of the column's 100 width, so it centres at
    // x = (100 - 40) / 2 = 30, and takes the top 30 of the 60-tall column.
    expectGeometry("centered", geometries[0], 30.0f, 30.0f, 40.0f, 30.0f);
}

// A guide alignment moves a widget in a shipped container, the placement that
// was inert before positioning became a solve. Two probes of different widths
// sit in a column; by default each centres across the column, so their left
// edges differ (40 at x = 30, 60 at x = 20). Aligning both left edges to one
// shared XGuide lines them up: the guide is a medium pull and the centring
// gravity only weak, so the guide wins and the two left edges coincide.
TEST(Layout, guideAlignmentMovesChildrenInAColumn)
{
    ProbeSet probes;

    XGuide g;

    Band const tall = { 30.0f, 30.0f, 30.0f };

    std::vector<AnyWidget> children;
    children.push_back(probes.add(Band{ 40.0f, 40.0f, 40.0f }, tall)
            | modifier::alignLeft(g));
    children.push_back(probes.add(Band{ 60.0f, 60.0f, 60.0f }, tall)
            | modifier::alignLeft(g));

    auto geometries = probes.realise(solverVbox(std::move(children)),
            avg::Vector2f(100.0f, 60.0f));

    ASSERT_EQ(2u, geometries.size());

    // Both left edges land on the one guide line. Their differing centred
    // defaults (30 and 20) would place them apart, so equal left edges prove the
    // guide moved at least one child off its gravity default.
    EXPECT_FLOAT_EQ(geometries[0]->position[0], geometries[1]->position[0]);
    EXPECT_FLOAT_EQ(40.0f, geometries[0]->size[0]);
    EXPECT_FLOAT_EQ(60.0f, geometries[1]->size[0]);
}

// A guide resolved above reaches across a makeWidgetWithSize firewall boundary
// as a constant. The outer column and the nested inner column are each their own
// firewall (every solver container is built through makeWidgetWithSize). The
// resolved-guide map, injected once at the top, threads down the BuildParams
// into both solves: a widget directly in the outer column and a widget inside
// the inner column both align their left edge to the one guide and land on the
// same line, though neither container resolves the guide itself. Each child is
// as wide as the container, so the inner firewall sits at the outer origin and
// the guide's window-space position carries across the boundary unshifted.
TEST(Layout, resolvedGuideCrossesFirewallBoundary)
{
    ProbeSet probes;

    XGuide g;

    Band const wide = { 200.0f, 200.0f, 200.0f };
    Band const tall = { 30.0f, 30.0f, 30.0f };

    std::vector<AnyWidget> innerChildren;
    innerChildren.push_back(probes.add(wide, tall) | modifier::alignLeft(g));

    std::vector<AnyWidget> outerChildren;
    outerChildren.push_back(probes.add(wide, tall) | modifier::alignLeft(g));
    outerChildren.push_back(solverVbox(std::move(innerChildren)));

    ResolvedGuideMap resolved{ { GuideAccess::id(g), 25.0f } };

    AnyWidget tree = solverVbox(std::move(outerChildren))
        | modifier::setParams<widget::ResolvedGuides>(resolved);

    auto geometries = probes.realise(std::move(tree),
            avg::Vector2f(200.0f, 100.0f));

    ASSERT_EQ(2u, geometries.size());

    // The outer widget's left edge is pinned to the inherited line at x = 25.
    ASSERT_TRUE(geometries[0].has_value());
    EXPECT_FLOAT_EQ(25.0f, geometries[0]->position[0]);
    // The inner firewall never resolves g, yet its widget lands on the same
    // line: the constant crossed the boundary through the resolved-guide map.
    ASSERT_TRUE(geometries[1].has_value());
    EXPECT_FLOAT_EQ(25.0f, geometries[1]->position[0]);
}

// Diagnostic added to localize a macOS-only failure of
// resolvedGuideCrossesFirewallBoundary, where both probes fall back to x = 0:
// the injected ResolvedGuides map never reaches the container's solve. setParams
// stores the map keyed by typeid(ResolvedGuides) in this test executable, while
// the container reads it back with provideParam<ResolvedGuides>() instantiated
// inside the bqui library. The whole project builds with hidden symbol
// visibility, under which a type's RTTI is emitted per binary, so the two
// typeid(ResolvedGuides) keys can differ across that boundary and the
// library-side BuildParams lookup then misses and defaults to an empty map. This
// reads the map size on both sides so the JUnit shows whether the map crosses
// the library boundary intact (both 1) or is dropped at the boundary (the
// library side 0), separating the plumbing from the solve.
TEST(Layout, resolvedGuidesCrossLibraryBoundary)
{
    XGuide g;

    ResolvedGuideMap injected{ { GuideAccess::id(g), 25.0f } };

    BuildParams params;
    params.set<widget::ResolvedGuides>(bq::signal::constant(injected));

    // Read in this executable, where the param was set: a same-binary round
    // trip that must always see the one entry.
    auto localContext = bq::signal::makeSignalContext(
            params.valueOrDefault<widget::ResolvedGuides>());
    std::size_t localCount = localContext.evaluate<0>().get<0>().size();

    // Read inside the bqui library, exactly as a container's solve does.
    std::size_t libraryCount = widget::resolvedGuideParamCount(params);

    EXPECT_EQ(1u, localCount)
        << "BuildParams set/get within the test executable dropped the "
           "injected ResolvedGuides map";
    EXPECT_EQ(1u, libraryCount)
        << "provideParam<ResolvedGuides>() inside the bqui library read "
        << libraryCount << " entries from a ResolvedGuides map this executable "
           "set to hold 1: the param did not survive the library boundary";
}

TEST(Layout, hboxAggregatesChildSizeHints)
{
    ProbeSet probes;

    Children children;
    children.push_back(probes.add(smallHint, fixed50));
    children.push_back(probes.add(stretchyHint, fixed50));
    children.push_back(probes.add(rigidHint, fixed50));

    auto builder = hbox(std::move(children))(BuildParams());

    auto context = bq::signal::makeSignalContext(builder.getSizeHint());
    SizeHint const& hint = context.evaluate<0>().get<0>();

    Band width = hint.getWidth().extent;
    EXPECT_FLOAT_EQ(60.0f, width.min);
    EXPECT_FLOAT_EQ(100.0f, width.natural);
    EXPECT_FLOAT_EQ(200.0f, width.max);

    // Across the layout axis the hints are combined by taking the largest.
    Band height = hint.getHeightForWidth(150.0f).extent;
    EXPECT_FLOAT_EQ(50.0f, height.min);
    EXPECT_FLOAT_EQ(50.0f, height.natural);
    EXPECT_FLOAT_EQ(50.0f, height.max);
}

// A baseline row lines its children up on a shared baseline. The two probes
// have different ascents (30 and 5) and different heights (40 and 30). The row's
// baseline sits at the largest ascent, 30 below the top, and each child keeps
// its natural height and hangs so its own baseline meets that line: the first
// child's top touches the row top (ascent 30 == the line) and the second sits 25
// below it (top at 30 - 5). In the y-up widget space, with the row 55 tall, both
// baselines land at y = 25.
TEST(Layout, baselineHboxAlignsChildrenOnTheirBaseline)
{
    ProbeSet probes;

    Band const wide = { 50.0f, 50.0f, 50.0f };

    std::vector<AnyWidget> children;
    children.push_back(probes.add(wide, Band{ 40.0f, 40.0f, 40.0f }, 30.0f));
    children.push_back(probes.add(wide, Band{ 30.0f, 30.0f, 30.0f }, 5.0f));

    auto geometries = probes.realise(baselineHbox(std::move(children)),
            avg::Vector2f(100.0f, 55.0f));

    ASSERT_EQ(2u, geometries.size());

    // The taller-ascent child fills the top of the row; the shorter one drops so
    // its baseline (5 below its own top) meets the same line.
    expectGeometry("ascent 30", geometries[0], 0.0f, 15.0f, 50.0f, 40.0f);
    expectGeometry("ascent 5", geometries[1], 50.0f, 0.0f, 50.0f, 30.0f);

    // The baselines coincide: each child's baseline is (row top) - ascent below
    // the top edge, which is y = 25 in the y-up space for both.
    float firstBaseline = geometries[0]->position[1]
        + geometries[0]->size[1] - 30.0f;
    float secondBaseline = geometries[1]->position[1]
        + geometries[1]->size[1] - 5.0f;
    EXPECT_FLOAT_EQ(25.0f, firstBaseline);
    EXPECT_FLOAT_EQ(firstBaseline, secondBaseline);
}

// The row reports its own band and baseline upward. Across the baseline the
// cross size is maxAscent + maxDescent = 30 + max(40 - 30, 30 - 5) = 55, and the
// row exposes its own first baseline at maxAscent = 30 so an enclosing baseline
// row could align on it. The main axis is unaffected: the widths still sum.
TEST(Layout, baselineHboxReportsRowHeightAndBaseline)
{
    ProbeSet probes;

    Band const wide = { 50.0f, 50.0f, 50.0f };

    std::vector<AnyWidget> children;
    children.push_back(probes.add(wide, Band{ 40.0f, 40.0f, 40.0f }, 30.0f));
    children.push_back(probes.add(wide, Band{ 30.0f, 30.0f, 30.0f }, 5.0f));

    auto builder = baselineHbox(std::move(children))(BuildParams());

    auto context = bq::signal::makeSignalContext(builder.getSizeHint());
    SizeHint const& hint = context.evaluate<0>().get<0>();

    AxisHint height = hint.getHeightForWidth(100.0f);
    EXPECT_FLOAT_EQ(55.0f, height.extent.min);
    EXPECT_FLOAT_EQ(55.0f, height.extent.natural);
    EXPECT_FLOAT_EQ(55.0f, height.extent.max);

    ASSERT_TRUE(height.anchors.firstBaseline.has_value());
    EXPECT_FLOAT_EQ(30.0f, *height.anchors.firstBaseline);

    Band width = hint.getWidth().extent;
    EXPECT_FLOAT_EQ(100.0f, width.min);
    EXPECT_FLOAT_EQ(100.0f, width.natural);
    EXPECT_FLOAT_EQ(100.0f, width.max);
}

// A child with no baseline in a baseline row falls back to the plain-row cross
// behaviour: it is given the row's full height as its slot and settles under
// gravity, while its baseline-bearing sibling still aligns. The baseline child
// (ascent 20, descent 20) sets the row height to 40; the plain child is handed
// that 40-tall slot and, wanting only 20, centres in it (y = 10).
TEST(Layout, baselineHboxSpansAChildWithoutABaseline)
{
    ProbeSet probes;

    Band const wide = { 50.0f, 50.0f, 50.0f };

    std::vector<AnyWidget> children;
    children.push_back(probes.add(wide, Band{ 40.0f, 40.0f, 40.0f }, 20.0f));
    children.push_back(probes.add(wide, Band{ 20.0f, 20.0f, 20.0f }));

    auto geometries = probes.realise(baselineHbox(std::move(children)),
            avg::Vector2f(100.0f, 40.0f));

    ASSERT_EQ(2u, geometries.size());

    // The baseline child keeps its natural 40 and fills the row height; the
    // plain child is centred in the 40-tall slot it is given.
    expectGeometry("baseline", geometries[0], 0.0f, 0.0f, 50.0f, 40.0f);
    expectGeometry("plain", geometries[1], 50.0f, 10.0f, 50.0f, 20.0f);
}

TEST(Layout, fixedChildrenKeepTheirSizeAtTheNaturalSize)
{
    ProbeSet probes;

    Children children;
    children.push_back(probes.add(rigidHint, fixed50));
    children.push_back(probes.add(rigidHint, fixed50));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(60.0f, 50.0f));

    ASSERT_EQ(2u, geometries.size());

    // A hint whose three entries are equal leaves getSizes with an empty
    // interval between the minimum and the natural size, so the multiplier for
    // that interval is computed from a zero denominator. It has to come out as
    // one; anything else scales every fixed-size child away.
    expectGeometry("first", geometries[0], 0.0f, 0.0f, 30.0f, 50.0f);
    expectGeometry("second", geometries[1], 30.0f, 0.0f, 30.0f, 50.0f);
}

TEST(Layout, fixedChildrenKeepTheirSizeInAnOversizedBox)
{
    ProbeSet probes;

    Children children;
    children.push_back(probes.add(rigidHint, fixed50));
    children.push_back(probes.add(rigidHint, fixed50));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(200.0f, 50.0f));

    ASSERT_EQ(2u, geometries.size());

    // The children have nothing to grow into, so the box keeps them at 30 each
    // and leaves the remaining 140 unused.
    expectGeometry("first", geometries[0], 0.0f, 0.0f, 30.0f, 50.0f);
    expectGeometry("second", geometries[1], 30.0f, 0.0f, 30.0f, 50.0f);
}

// Content wider than the container, no filler: the horizontal twin of
// solverVboxOverflowsOverfullContent. With the required containment cap
// dropped the row overflows instead of squashing -- each child keeps its width
// and the last extends past the container's right edge.
TEST(Layout, hboxOverflowsOverfullContent)
{
    Band const height = { 50.0f, 50.0f, 50.0f };
    Band const fixed40 = { 40.0f, 40.0f, 40.0f };

    avg::Vector2f const size(40.0f, 50.0f);

    ProbeSet solverProbes;
    Children children;
    children.push_back(solverProbes.add(fixed40, height));
    children.push_back(solverProbes.add(fixed40, height));
    auto solved = solverProbes.realise(hbox(std::move(children)), size);

    ASSERT_EQ(2u, solved.size());

    // Both children keep their 40. The first packs against the left; the second
    // begins where it ends and overflows past the container's right edge.
    expectGeometry("first", solved[0], 0.0f, 0.0f, 40.0f, 50.0f);
    expectGeometry("second", solved[1], 40.0f, 0.0f, 40.0f, 50.0f);
}

// A row laid out through the arrange solver, the horizontal twin of
// solverVboxStacksChildrenThroughTheSolver. Three rigid children whose widths
// fill the container exactly leave the solve fully determined: the first packs
// against the left, each meets the next, and the last ends at the container's
// right. This proves the solver drives real hbox geometry end to end.
TEST(Layout, hboxStacksChildrenThroughTheSolver)
{
    Band const height = { 50.0f, 50.0f, 50.0f };
    Band const a = { 40.0f, 40.0f, 40.0f };
    Band const b = { 30.0f, 30.0f, 30.0f };
    Band const c = { 50.0f, 50.0f, 50.0f };

    avg::Vector2f const size(120.0f, 50.0f);

    ProbeSet solverProbes;
    Children children;
    children.push_back(solverProbes.add(a, height));
    children.push_back(solverProbes.add(b, height));
    children.push_back(solverProbes.add(c, height));
    auto solved = solverProbes.realise(hbox(std::move(children)), size);

    ASSERT_EQ(3u, solved.size());

    // The row is packed left to right, edge to edge.
    expectGeometry("first", solved[0], 0.0f, 0.0f, 40.0f, 50.0f);
    expectGeometry("middle", solved[1], 40.0f, 0.0f, 30.0f, 50.0f);
    expectGeometry("last", solved[2], 70.0f, 0.0f, 50.0f, 50.0f);
}

// Content narrower than the container, no filler: the horizontal twin of
// solverVboxPacksContentShorterThanContainerAtTheTop. The weak fill keeps the
// medium natural widths winning, so the row packs against the left and leaves
// the spare space as a gap on the right.
TEST(Layout, hboxPacksContentShorterThanContainerAtTheStart)
{
    Band const height = { 50.0f, 50.0f, 50.0f };
    Band const a = { 40.0f, 40.0f, 40.0f };
    Band const b = { 30.0f, 30.0f, 30.0f };
    Band const c = { 50.0f, 50.0f, 50.0f };

    avg::Vector2f const size(150.0f, 50.0f);

    ProbeSet solverProbes;
    Children children;
    children.push_back(solverProbes.add(a, height));
    children.push_back(solverProbes.add(b, height));
    children.push_back(solverProbes.add(c, height));
    auto solved = solverProbes.realise(hbox(std::move(children)), size);

    ASSERT_EQ(3u, solved.size());

    // Each rigid child at its natural width, packed left with the spare 30 left
    // as a gap on the right.
    expectGeometry("first", solved[0], 0.0f, 0.0f, 40.0f, 50.0f);
    expectGeometry("middle", solved[1], 40.0f, 0.0f, 30.0f, 50.0f);
    expectGeometry("last", solved[2], 70.0f, 0.0f, 50.0f, 50.0f);
}

// Two fillers with a rigid child to their left: the horizontal twin of
// solverVboxSplitsLeftoverBetweenFillers. Both fillers share the container's
// single stretch variable, so the weak fill splits the 100 of leftover space
// equally.
TEST(Layout, hboxSplitsLeftoverBetweenFillers)
{
    Band const height = { 50.0f, 50.0f, 50.0f };
    Band const rigid = { 40.0f, 40.0f, 40.0f };

    avg::Vector2f const size(140.0f, 50.0f);

    ProbeSet solverProbes;
    Children children;
    children.push_back(solverProbes.add(rigid, height));
    children.push_back(solverProbes.add(fillHint, height));
    children.push_back(solverProbes.add(fillHint, height));
    auto solved = solverProbes.realise(hbox(std::move(children)), size);

    ASSERT_EQ(3u, solved.size());

    // Rigid child keeps its 40 at the left; the two fillers split the remaining
    // 100 into 50 each.
    expectGeometry("rigid", solved[0], 0.0f, 0.0f, 40.0f, 50.0f);
    expectGeometry("filler one", solved[1], 40.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("filler two", solved[2], 90.0f, 0.0f, 50.0f, 50.0f);
}

// A single filler under the weak fill takes exactly the space the rigid child
// leaves: the horizontal twin of solverVboxFillsASingleFiller. A 40 rigid child
// to the left of a filler in a 150-wide box leaves the filler 110.
TEST(Layout, hboxFillsASingleFiller)
{
    ProbeSet probes;

    Band const height = { 50.0f, 50.0f, 50.0f };

    Children children;
    children.push_back(probes.add(Band{ 40.0f, 40.0f, 40.0f }, height));
    children.push_back(probes.add(fillHint, height));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(150.0f, 50.0f));

    ASSERT_EQ(2u, geometries.size());

    expectGeometry("rigid", geometries[0], 0.0f, 0.0f, 40.0f, 50.0f);
    expectGeometry("filler", geometries[1], 40.0f, 0.0f, 110.0f, 50.0f);
}

// A grow=2 filler takes twice the surplus of a grow=1 filler beside it. With no
// rigid content and both naturals zero, the two split the 90-wide row 60/30.
TEST(Layout, hboxWeightsFillersByGrow)
{
    ProbeSet probes;

    Band const grow2 = { 0.0f, 0.0f, 100000.0f, 2.0f };
    Band const grow1 = { 0.0f, 0.0f, 100000.0f, 1.0f };

    Children children;
    children.push_back(probes.add(grow2, fixed50));
    children.push_back(probes.add(grow1, fixed50));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(90.0f, 50.0f));

    ASSERT_EQ(2u, geometries.size());

    expectGeometry("grow2", geometries[0], 0.0f, 0.0f, 60.0f, 50.0f);
    expectGeometry("grow1", geometries[1], 60.0f, 0.0f, 30.0f, 50.0f);
}

// Two grow=1 fillers split the surplus equally even when their naturals differ:
// each keeps its natural and takes the same share of what is left. Naturals 20
// and 40 cost 60 of the 100-wide row, so the 40 surplus splits 20/20 and the
// children settle at 40 and 60.
TEST(Layout, hboxSplitsSurplusEquallyOverNaturals)
{
    ProbeSet probes;

    Band const a = { 0.0f, 20.0f, 100000.0f, 1.0f };
    Band const b = { 0.0f, 40.0f, 100000.0f, 1.0f };

    Children children;
    children.push_back(probes.add(a, fixed50));
    children.push_back(probes.add(b, fixed50));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(100.0f, 50.0f));

    ASSERT_EQ(2u, geometries.size());

    expectGeometry("a", geometries[0], 0.0f, 0.0f, 40.0f, 50.0f);
    expectGeometry("b", geometries[1], 40.0f, 0.0f, 60.0f, 50.0f);
}

TEST(Layout, hboxPlacesASingleStretchingChild)
{
    ProbeSet probes;

    Children children;
    children.push_back(probes.add(
                Band{ 10.0f, 20.0f, 100.0f, 1.0f },
                fillHint
                ));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(60.0f, 50.0f));

    ASSERT_EQ(1u, geometries.size());

    // 60 covers the minimum, the natural size and half of the filler range, so
    // the only child takes the whole box.
    expectGeometry("only", geometries[0], 0.0f, 0.0f, 60.0f, 50.0f);
}

TEST(Layout, hboxGivesZeroSizedChildrenNoRoom)
{
    ProbeSet probes;

    Band const zero = { 0.0f, 0.0f, 0.0f };

    Children children;
    children.push_back(probes.add(zero, zero));
    children.push_back(probes.add(zero, zero));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(100.0f, 50.0f));

    ASSERT_EQ(2u, geometries.size());

    // Both children are realised with an empty size at the same spot, centered
    // vertically in the full-height slot the hbox gave them.
    expectGeometry("first", geometries[0], 0.0f, 25.0f, 0.0f, 0.0f);
    expectGeometry("second", geometries[1], 0.0f, 25.0f, 0.0f, 0.0f);
}

TEST(Layout, emptyBoxHasNoChildrenAndAZeroSizeHint)
{
    auto builder = hbox({})(BuildParams());

    auto sizeHint = builder.getSizeHint();
    auto instanceSignal = std::move(builder)(
            bq::signal::constant(avg::Vector2f(100.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(sizeHint),
            std::move(instanceSignal));

    SizeHint const& hint = context.evaluate<0>().get<0>();
    Instance const& instance = context.evaluate<1>().get<0>();

    Band width = hint.getWidth().extent;
    EXPECT_FLOAT_EQ(0.0f, width.min);
    EXPECT_FLOAT_EQ(0.0f, width.natural);
    EXPECT_FLOAT_EQ(0.0f, width.max);

    Band height = hint.getHeightForWidth(100.0f).extent;
    EXPECT_FLOAT_EQ(0.0f, height.min);
    EXPECT_FLOAT_EQ(0.0f, height.natural);
    EXPECT_FLOAT_EQ(0.0f, height.max);

    EXPECT_TRUE(instance.getInputAreas().empty());
}

// A hint whose natural size is below its minimum is read as if its natural
// were raised to the minimum, so the children still share the container
// between them rather than being handed more room than there is.
TEST(Layout, hboxFitsNonMonotonicHintsInTheContainer)
{
    ProbeSet probes;

    Band const nonMonotonic = { 100.0f, 0.0f, 100.0f };

    Children children;
    children.push_back(probes.add(nonMonotonic, fixed50));
    children.push_back(probes.add(nonMonotonic, fixed50));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(200.0f, 50.0f));

    ASSERT_EQ(2u, geometries.size());

    expectGeometry("first", geometries[0], 0.0f, 0.0f, 100.0f, 50.0f);
    expectGeometry("second", geometries[1], 100.0f, 0.0f, 100.0f, 50.0f);
}

// The aggregate is read the way getSizes reads the children, so at its own
// natural width the box can give the second child its natural 50.
TEST(Layout, hboxAggregatesNonMonotonicHintsAsItAllocates)
{
    ProbeSet probes;

    Children hinted;
    hinted.push_back(probes.add(Band{ 100.0f, 0.0f, 100.0f }, fixed50));
    hinted.push_back(probes.add(Band{ 0.0f, 50.0f, 50.0f }, fixed50));

    auto builder = hbox(std::move(hinted))(BuildParams());

    auto context = bq::signal::makeSignalContext(builder.getSizeHint());
    SizeHint const& hint = context.evaluate<0>().get<0>();

    Band width = hint.getWidth().extent;
    EXPECT_FLOAT_EQ(100.0f, width.min);
    EXPECT_FLOAT_EQ(150.0f, width.natural);
    EXPECT_FLOAT_EQ(150.0f, width.max);

    ProbeSet rowProbes;

    Children children;
    children.push_back(rowProbes.add(Band{ 100.0f, 0.0f, 100.0f }, fixed50));
    children.push_back(rowProbes.add(Band{ 0.0f, 50.0f, 50.0f }, fixed50));

    auto geometries = rowProbes.realise(hbox(std::move(children)),
            avg::Vector2f(width.natural, 50.0f));

    ASSERT_EQ(2u, geometries.size());

    expectGeometry("first", geometries[0], 0.0f, 0.0f, 100.0f, 50.0f);
    expectGeometry("second", geometries[1], 100.0f, 0.0f, 50.0f, 50.0f);
}

// A maximum below the minimum does not pull a child below its minimum. The
// minimums below total exactly the size, so each child is given its minimum.
TEST(Layout, getSizesKeepsTheMinimumAboveASmallerMaximum)
{
    std::vector<AxisHint> hints {
        AxisHint{ Band{ 50.0f, 80.0f, 20.0f }, Anchors{} },
        AxisHint{ Band{ 10.0f, 10.0f, 10.0f }, Anchors{} }
    };

    auto sizes = getSizes(60.0f, hints);

    ASSERT_EQ(2u, sizes.size());

    EXPECT_FLOAT_EQ(50.0f, sizes[0]);
    EXPECT_FLOAT_EQ(10.0f, sizes[1]);
}

TEST(Layout, stackGivesEveryChildTheContainerSize)
{
    ProbeSet probes;

    std::vector<AnyWidget> children;
    children.push_back(probes.add(fillHint, fillHint));
    children.push_back(probes.add(fixed50, rigidHint));

    auto geometries = probes.realise(stack(std::move(children)),
            avg::Vector2f(200.0f, 100.0f));

    ASSERT_EQ(2u, geometries.size());

    // Both children are offered the full 200x100. The stretching one takes all
    // of it; the fixed one keeps 50x30 and gravity centers it in the same slot.
    expectGeometry("stretching", geometries[0], 0.0f, 0.0f, 200.0f, 100.0f);
    expectGeometry("fixed", geometries[1], 75.0f, 35.0f, 50.0f, 30.0f);
}

TEST(Layout, stackAggregatesChildSizeHintsByMaximum)
{
    ProbeSet probes;

    std::vector<AnyWidget> children;
    children.push_back(probes.add(
                Band{ 40.0f, 50.0f, 90.0f },
                Band{ 10.0f, 30.0f, 30.0f }
                ));
    children.push_back(probes.add(
                Band{ 10.0f, 80.0f, 30.0f },
                Band{ 20.0f, 60.0f, 60.0f }
                ));

    auto builder = stack(std::move(children))(BuildParams());

    auto context = bq::signal::makeSignalContext(builder.getSizeHint());
    SizeHint const& hint = context.evaluate<0>().get<0>();

    // The maximum is taken entry by entry, so the aggregate width matches
    // neither child.
    Band width = hint.getWidth().extent;
    EXPECT_FLOAT_EQ(40.0f, width.min);
    EXPECT_FLOAT_EQ(80.0f, width.natural);
    EXPECT_FLOAT_EQ(90.0f, width.max);

    Band height = hint.getHeightForWidth(200.0f).extent;
    EXPECT_FLOAT_EQ(20.0f, height.min);
    EXPECT_FLOAT_EQ(60.0f, height.natural);
    EXPECT_FLOAT_EQ(60.0f, height.max);
}

TEST(Layout, uniformGridPlacesCellsFromTheBottomLeft)
{
    ProbeSet probes;

    auto bottomLeft = probes.add(fillHint, fillHint);
    auto bottomRight = probes.add(fillHint, fillHint);
    auto topRow = probes.add(fillHint, fillHint);

    AnyWidget grid = uniformGrid(2, 2)
        .cell(0, 0, 1, 1, std::move(bottomLeft))
        .cell(1, 0, 1, 1, std::move(bottomRight))
        .cell(0, 1, 2, 1, std::move(topRow))
        ;

    auto geometries = probes.realise(std::move(grid),
            avg::Vector2f(200.0f, 100.0f));

    ASSERT_EQ(3u, geometries.size());

    // Cells are 100x50. Unlike vbox, the grid's rows grow upwards from the
    // origin, so row 0 is the bottom one.
    expectGeometry("bottom left", geometries[0], 0.0f, 0.0f, 100.0f, 50.0f);
    expectGeometry("bottom right", geometries[1], 100.0f, 0.0f, 100.0f, 50.0f);
    expectGeometry("top row", geometries[2], 0.0f, 50.0f, 200.0f, 50.0f);
}

// The child below spans the whole 2x2 grid and so receives the container's
// full size, so the container asks for exactly what the child wants.
TEST(Layout, uniformGridSizeHintAccountsForCellSpans)
{
    ProbeSet probes;

    AnyWidget grid = uniformGrid(2, 2)
        .cell(0, 0, 2, 2, probes.add(
                    Band{ 10.0f, 20.0f, 30.0f },
                    Band{ 5.0f, 10.0f, 15.0f }
                    ))
        ;

    auto builder = std::move(grid)(BuildParams());

    auto context = bq::signal::makeSignalContext(builder.getSizeHint());
    SizeHint const& hint = context.evaluate<0>().get<0>();

    Band width = hint.getWidth().extent;
    EXPECT_FLOAT_EQ(10.0f, width.min);
    EXPECT_FLOAT_EQ(20.0f, width.natural);
    EXPECT_FLOAT_EQ(30.0f, width.max);

    Band height = hint.getHeightForWidth(40.0f).extent;
    EXPECT_FLOAT_EQ(5.0f, height.min);
    EXPECT_FLOAT_EQ(10.0f, height.natural);
    EXPECT_FLOAT_EQ(15.0f, height.max);
}

// Each child's hint is spread over the cells it spans, and the grid needs the
// largest per-cell share on every cell. The row below wants 30 across two
// columns, 15 a column, which outweighs the 10 of the single cell under it.
TEST(Layout, uniformGridSizeHintTakesTheLargestShareOfACell)
{
    ProbeSet probes;

    AnyWidget grid = uniformGrid(2, 2)
        .cell(0, 0, 1, 1, probes.add(
                    Band{ 10.0f, 10.0f, 10.0f },
                    Band{ 40.0f, 40.0f, 40.0f }
                    ))
        .cell(0, 1, 2, 1, probes.add(
                    Band{ 30.0f, 30.0f, 30.0f },
                    Band{ 10.0f, 10.0f, 10.0f }
                    ))
        ;

    auto builder = std::move(grid)(BuildParams());

    auto context = bq::signal::makeSignalContext(builder.getSizeHint());
    SizeHint const& hint = context.evaluate<0>().get<0>();

    Band width = hint.getWidth().extent;
    EXPECT_FLOAT_EQ(30.0f, width.min);
    EXPECT_FLOAT_EQ(30.0f, width.natural);
    EXPECT_FLOAT_EQ(30.0f, width.max);

    // Rows are the other way round: the single cell's 40 outweighs the row's
    // 10, so each of the two rows needs 40.
    Band height = hint.getHeightForWidth(30.0f).extent;
    EXPECT_FLOAT_EQ(80.0f, height.min);
    EXPECT_FLOAT_EQ(80.0f, height.natural);
    EXPECT_FLOAT_EQ(80.0f, height.max);
}

// A child spanning two of three columns is asked for its height at two thirds
// of the grid's width, and likewise for rows.
TEST(Layout, uniformGridQueriesAChildAtTheSizeOfItsSpan)
{
    auto queried = std::make_shared<float>(0.0f);

    AnyWidget child = makeWidget()
        | modifier::setSizeHint(bq::signal::constant(
                    SizeHint(QueryRecordingHint{ queried })))
        ;

    AnyWidget grid = uniformGrid(3, 3)
        .cell(0, 0, 2, 2, std::move(child))
        ;

    auto builder = std::move(grid)(BuildParams());

    auto context = bq::signal::makeSignalContext(builder.getSizeHint());
    SizeHint const& hint = context.evaluate<0>().get<0>();

    Band height = hint.getHeightForWidth(60.0f).extent;
    EXPECT_FLOAT_EQ(40.0f, *queried);
    EXPECT_FLOAT_EQ(60.0f, height.natural);

    Band width = hint.getWidthForHeight(60.0f).extent;
    EXPECT_FLOAT_EQ(40.0f, *queried);
    EXPECT_FLOAT_EQ(60.0f, width.natural);
}

TEST(Layout, uniformGridRejectsAnEmptyDimension)
{
    EXPECT_THROW(uniformGrid(0, 2), std::invalid_argument);
    EXPECT_THROW(uniformGrid(2, 0), std::invalid_argument);
    EXPECT_NO_THROW(uniformGrid(1, 1));
}

TEST(Layout, uniformGridRejectsACellPastItsEdge)
{
    EXPECT_THROW(uniformGrid(2, 2).cell(2, 0, 1, 1, makeWidget()),
            std::invalid_argument);
    EXPECT_THROW(uniformGrid(2, 2).cell(0, 2, 1, 1, makeWidget()),
            std::invalid_argument);
    EXPECT_THROW(uniformGrid(2, 2).cell(1, 0, 2, 1, makeWidget()),
            std::invalid_argument);
    EXPECT_THROW(uniformGrid(2, 2).cell(0, 1, 1, 2, makeWidget()),
            std::invalid_argument);
    EXPECT_THROW(uniformGrid(2, 2).cell(1, 0, ~0u, 1, makeWidget()),
            std::invalid_argument);
    EXPECT_NO_THROW(uniformGrid(2, 2).cell(0, 0, 2, 2, makeWidget()));
    EXPECT_NO_THROW(uniformGrid(2, 2).cell(1, 1, 1, 1, makeWidget()));
}

TEST(Layout, nestedBoxesComposeTransforms)
{
    ProbeSet probes;

    Children row;
    row.push_back(probes.add(fixed50, fillHint));
    row.push_back(probes.add(fixed50, fillHint));

    Children column;
    column.push_back(hbox(std::move(row)));
    column.push_back(probes.add(fillHint, rigidHint));

    auto geometries = probes.realise(vbox(std::move(column)),
            avg::Vector2f(200.0f, 100.0f));

    ASSERT_EQ(3u, geometries.size());

    // The vbox gives the row the top 70 and the footer the bottom 30. The row
    // wants only 100 of the 200 available width, so gravity centers it at
    // x = 50; a leaf's position is that offset plus its own place in the row.
    expectGeometry("first in row", geometries[0], 50.0f, 30.0f, 50.0f, 70.0f);
    expectGeometry("second in row", geometries[1], 100.0f, 30.0f, 50.0f, 70.0f);
    expectGeometry("footer", geometries[2], 0.0f, 0.0f, 200.0f, 30.0f);
}

TEST(Layout, dynamicHboxPlacesChildrenLeftToRight)
{
    ProbeSet probes;

    probes.addIndexed(fixed100, fixed50);
    probes.addIndexed(fixed50, fixed50);

    auto input = bq::signal::makeInput(std::vector<size_t>{ 0, 1 });

    auto instanceSignal = hbox(dynamicChildren(probes, input.signal))(
            BuildParams())(
            bq::signal::constant(avg::Vector2f(300.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    ASSERT_EQ(2u, geometries.size());

    expectGeometry("first", geometries[0], 0.0f, 0.0f, 100.0f, 50.0f);
    expectGeometry("second", geometries[1], 100.0f, 0.0f, 50.0f, 50.0f);
}

TEST(Layout, dynamicHboxPlacesAnAddedChild)
{
    ProbeSet probes;

    probes.addIndexed(fixed100, fixed50);
    probes.addIndexed(fixed50, fixed50);

    auto input = bq::signal::makeInput(std::vector<size_t>{ 0, 1 });

    auto instanceSignal = hbox(dynamicChildren(probes, input.signal))(
            BuildParams())(
            bq::signal::constant(avg::Vector2f(300.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    size_t added = probes.addIndexed(fixed150, fixed50);

    input.handle.set(std::vector<size_t>{ 0, 1, added });
    context.update(nextFrame(1));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    ASSERT_EQ(3u, geometries.size());

    expectGeometry("first", geometries[0], 0.0f, 0.0f, 100.0f, 50.0f);
    expectGeometry("second", geometries[1], 100.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("added", geometries[2], 150.0f, 0.0f, 150.0f, 50.0f);
}

TEST(Layout, dynamicHboxDropsARemovedChild)
{
    ProbeSet probes;

    probes.addIndexed(fixed100, fixed50);
    probes.addIndexed(fixed50, fixed50);
    probes.addIndexed(fixed150, fixed50);

    auto input = bq::signal::makeInput(std::vector<size_t>{ 0, 1, 2 });

    auto instanceSignal = hbox(dynamicChildren(probes, input.signal))(
            BuildParams())(
            bq::signal::constant(avg::Vector2f(300.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    input.handle.set(std::vector<size_t>{ 0, 2 });
    context.update(nextFrame(1));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    ASSERT_EQ(3u, geometries.size());

    expectGeometry("first", geometries[0], 0.0f, 0.0f, 100.0f, 50.0f);
    expectNotRealised("removed", geometries[1]);
    expectGeometry("last", geometries[2], 100.0f, 0.0f, 150.0f, 50.0f);
}

TEST(Layout, dynamicHboxFollowsReorderedKeys)
{
    ProbeSet probes;

    probes.addIndexed(fixed100, fixed50);
    probes.addIndexed(fixed50, fixed50);
    probes.addIndexed(fixed150, fixed50);

    auto input = bq::signal::makeInput(std::vector<size_t>{ 0, 1, 2 });

    auto instanceSignal = hbox(dynamicChildren(probes, input.signal))(
            BuildParams())(
            bq::signal::constant(avg::Vector2f(300.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    input.handle.set(std::vector<size_t>{ 2, 0, 1 });
    context.update(nextFrame(1));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    ASSERT_EQ(3u, geometries.size());

    // Every child keeps its key, so nothing is rebuilt and each one moves to
    // the slot its new position in the list earns it.
    expectGeometry("first", geometries[0], 150.0f, 0.0f, 100.0f, 50.0f);
    expectGeometry("second", geometries[1], 250.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("last", geometries[2], 0.0f, 0.0f, 150.0f, 50.0f);
}

// The probe asks for 50x30 and gets a 50x100 slot, so it is centered in it.
TEST(Layout, dynamicHboxAppliesGravity)
{
    ProbeSet probes;

    probes.addIndexed(fixed50, rigidHint);

    auto input = bq::signal::makeInput(std::vector<size_t>{ 0 });

    auto instanceSignal = hbox(dynamicChildren(probes, input.signal))(
            BuildParams())(
            bq::signal::constant(avg::Vector2f(300.0f, 100.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    ASSERT_EQ(1u, geometries.size());

    expectGeometry("only", geometries[0], 0.0f, 35.0f, 50.0f, 30.0f);
}

// Adding or removing children does not rebuild the others. The per-child build
// count is measured from the first pass.
TEST(Layout, dynamicHboxBuildsEachChildOncePerIdentity)
{
    ProbeSet probes;

    probes.addIndexed(fixed100, fixed50);
    probes.addIndexed(fixed50, fixed50);

    auto builds = std::make_shared<int>(0);

    auto input = bq::signal::makeInput(std::vector<size_t>{ 0, 1 });

    auto instanceSignal = hbox(dynamicChildren(probes, input.signal, builds))(
            BuildParams())(
            bq::signal::constant(avg::Vector2f(300.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    ASSERT_GT(*builds, 0);
    ASSERT_EQ(0, *builds % 2);

    int const perChild = *builds / 2;

    size_t added = probes.addIndexed(fixed150, fixed50);

    input.handle.set(std::vector<size_t>{ added, 0, 1 });
    context.update(nextFrame(1));

    // Only the new child is built.
    EXPECT_EQ(3 * perChild, *builds);

    input.handle.set(std::vector<size_t>{ 1, added });
    context.update(nextFrame(2));

    EXPECT_EQ(3 * perChild, *builds);
}

// One braced list mixes constant children with two collections. Each segment
// keeps its place in the written order as its collection changes, and a change
// builds only the item it adds.
TEST(Layout, hboxMixesConstantsAndCollections)
{
    ProbeSet probes;

    size_t const first = probes.addIndexed(fixed50, fixed50);
    size_t const a0 = probes.addIndexed(fixed50, fixed50);
    size_t const a1 = probes.addIndexed(fixed50, fixed50);
    size_t const middle = probes.addIndexed(fixed50, fixed50);
    size_t const b0 = probes.addIndexed(fixed50, fixed50);
    size_t const b1 = probes.addIndexed(fixed50, fixed50);

    auto as = makeIndices({ a0, a1 });
    auto bs = makeIndices({ b0, b1 });

    auto constantBuilds = std::make_shared<int>(0);
    auto aBuilds = std::make_shared<int>(0);
    auto bBuilds = std::make_shared<int>(0);

    auto constant = [&](size_t index)
    {
        return countBuilds(constantBuilds,
                probes.fromSignal(bq::signal::constant(index)));
    };

    auto instanceSignal = hbox({
            constant(first),
            collectionChildren(probes, as, aBuilds),
            constant(middle),
            collectionChildren(probes, bs, bBuilds)
            })(BuildParams())(
            bq::signal::constant(avg::Vector2f(1000.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    expectGeometry("first", geometries[first], 0.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("a0", geometries[a0], 50.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("a1", geometries[a1], 100.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("middle", geometries[middle], 150.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("b0", geometries[b0], 200.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("b1", geometries[b1], 250.0f, 0.0f, 50.0f, 50.0f);

    ASSERT_GT(*constantBuilds, 0);
    ASSERT_EQ(0, *constantBuilds % 2);

    int const perChild = *constantBuilds / 2;

    EXPECT_EQ(2 * perChild, *aBuilds);
    EXPECT_EQ(2 * perChild, *bBuilds);

    size_t const b2 = probes.addIndexed(fixed50, fixed50);
    bs.write().pushFront(b2);
    context.update(nextFrame(1));

    geometries = probes.read(context.evaluate<0>().get<0>());

    expectGeometry("first", geometries[first], 0.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("a0", geometries[a0], 50.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("a1", geometries[a1], 100.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("middle", geometries[middle], 150.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("b2", geometries[b2], 200.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("b0", geometries[b0], 250.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("b1", geometries[b1], 300.0f, 0.0f, 50.0f, 50.0f);

    EXPECT_EQ(2 * perChild, *constantBuilds);
    EXPECT_EQ(2 * perChild, *aBuilds);
    EXPECT_EQ(3 * perChild, *bBuilds);

    size_t const a2 = probes.addIndexed(fixed50, fixed50);
    {
        auto transaction = as.write();
        transaction.erase(transaction.items().begin());
        transaction.pushBack(a2);
    }
    context.update(nextFrame(2));

    geometries = probes.read(context.evaluate<0>().get<0>());

    expectGeometry("first", geometries[first], 0.0f, 0.0f, 50.0f, 50.0f);
    expectNotRealised("a0", geometries[a0]);
    expectGeometry("a1", geometries[a1], 50.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("a2", geometries[a2], 100.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("middle", geometries[middle], 150.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("b2", geometries[b2], 200.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("b0", geometries[b0], 250.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("b1", geometries[b1], 300.0f, 0.0f, 50.0f, 50.0f);

    EXPECT_EQ(2 * perChild, *constantBuilds);
    EXPECT_EQ(3 * perChild, *aBuilds);
    EXPECT_EQ(3 * perChild, *bBuilds);

    {
        auto transaction = bs.write();
        transaction.erase(transaction.items().begin());
    }
    context.update(nextFrame(3));

    geometries = probes.read(context.evaluate<0>().get<0>());

    expectGeometry("middle", geometries[middle], 150.0f, 0.0f, 50.0f, 50.0f);
    expectNotRealised("b2", geometries[b2]);
    expectGeometry("b0", geometries[b0], 200.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("b1", geometries[b1], 250.0f, 0.0f, 50.0f, 50.0f);

    EXPECT_EQ(2 * perChild, *constantBuilds);
    EXPECT_EQ(3 * perChild, *aBuilds);
    EXPECT_EQ(3 * perChild, *bBuilds);
}

TEST(Layout, vboxMixesConstantsAndCollections)
{
    ProbeSet probes;

    size_t const first = probes.addIndexed(fixed50, fixed50);
    size_t const a0 = probes.addIndexed(fixed50, fixed50);
    size_t const middle = probes.addIndexed(fixed50, fixed50);
    size_t const b0 = probes.addIndexed(fixed50, fixed50);

    auto as = makeIndices({ a0 });
    auto bs = makeIndices({ b0 });

    auto builds = std::make_shared<int>(0);

    auto instanceSignal = vbox({
            probes.fromSignal(bq::signal::constant(first)),
            collectionChildren(probes, as, builds),
            probes.fromSignal(bq::signal::constant(middle)),
            collectionChildren(probes, bs, builds)
            })(BuildParams())(
            bq::signal::constant(avg::Vector2f(50.0f, 250.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    size_t const b1 = probes.addIndexed(fixed50, fixed50);
    bs.write().pushBack(b1);
    context.update(nextFrame(1));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    expectGeometry("first", geometries[first], 0.0f, 200.0f, 50.0f, 50.0f);
    expectGeometry("a0", geometries[a0], 0.0f, 150.0f, 50.0f, 50.0f);
    expectGeometry("middle", geometries[middle], 0.0f, 100.0f, 50.0f, 50.0f);
    expectGeometry("b0", geometries[b0], 0.0f, 50.0f, 50.0f, 50.0f);
    expectGeometry("b1", geometries[b1], 0.0f, 0.0f, 50.0f, 50.0f);
}

TEST(Layout, hboxKeepsAnEmptyCollectionInPlace)
{
    ProbeSet probes;

    size_t const first = probes.addIndexed(fixed50, fixed50);
    size_t const last = probes.addIndexed(fixed50, fixed50);

    auto items = makeIndices({});

    auto builds = std::make_shared<int>(0);

    auto instanceSignal = hbox({
            probes.fromSignal(bq::signal::constant(first)),
            collectionChildren(probes, items, builds),
            probes.fromSignal(bq::signal::constant(last))
            })(BuildParams())(
            bq::signal::constant(avg::Vector2f(300.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    expectGeometry("first", geometries[first], 0.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("last", geometries[last], 50.0f, 0.0f, 50.0f, 50.0f);

    size_t const added = probes.addIndexed(fixed50, fixed50);
    items.write().pushBack(added);
    context.update(nextFrame(1));

    geometries = probes.read(context.evaluate<0>().get<0>());

    expectGeometry("first", geometries[first], 0.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("added", geometries[added], 50.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("last", geometries[last], 100.0f, 0.0f, 50.0f, 50.0f);
}

// Two lists over one collection mint identities of their own, so each keeps
// its own children and builds its own copy of an added item.
TEST(Layout, hboxListsOverOneCollectionKeepTheirOwnChildren)
{
    ProbeSet probes;

    size_t const left0 = probes.addIndexed(fixed50, fixed50);
    size_t const left1 = probes.addIndexed(fixed50, fixed50);
    size_t const left2 = probes.addIndexed(fixed50, fixed50);
    size_t const right0 = probes.addIndexed(fixed50, fixed50);
    size_t const right1 = probes.addIndexed(fixed50, fixed50);
    size_t const right2 = probes.addIndexed(fixed50, fixed50);
    size_t const middle = probes.addIndexed(fixed50, fixed50);

    size_t const offset = right0 - left0;

    auto items = makeIndices({ left0, left1 });

    auto leftBuilds = std::make_shared<int>(0);
    auto rightBuilds = std::make_shared<int>(0);

    auto instanceSignal = hbox({
            collectionChildren(probes, items, leftBuilds),
            probes.fromSignal(bq::signal::constant(middle)),
            collectionChildren(probes, items, rightBuilds, offset)
            })(BuildParams())(
            bq::signal::constant(avg::Vector2f(1000.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    ASSERT_GT(*leftBuilds, 0);
    ASSERT_EQ(0, *leftBuilds % 2);
    ASSERT_EQ(*leftBuilds, *rightBuilds);

    int const perChild = *leftBuilds / 2;

    items.write().pushBack(left2);
    context.update(nextFrame(1));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    expectGeometry("left0", geometries[left0], 0.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("left1", geometries[left1], 50.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("left2", geometries[left2], 100.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("middle", geometries[middle], 150.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("right0", geometries[right0], 200.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("right1", geometries[right1], 250.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("right2", geometries[right2], 300.0f, 0.0f, 50.0f, 50.0f);

    EXPECT_EQ(3 * perChild, *leftBuilds);
    EXPECT_EQ(3 * perChild, *rightBuilds);

    {
        auto transaction = items.write();
        transaction.erase(transaction.items().begin());
    }
    context.update(nextFrame(2));

    geometries = probes.read(context.evaluate<0>().get<0>());

    expectNotRealised("left0", geometries[left0]);
    expectGeometry("left1", geometries[left1], 0.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("left2", geometries[left2], 50.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("middle", geometries[middle], 100.0f, 0.0f, 50.0f, 50.0f);
    expectNotRealised("right0", geometries[right0]);
    expectGeometry("right1", geometries[right1], 150.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("right2", geometries[right2], 200.0f, 0.0f, 50.0f, 50.0f);

    EXPECT_EQ(3 * perChild, *leftBuilds);
    EXPECT_EQ(3 * perChild, *rightBuilds);
}

TEST(Layout, handleGravityEvaluatesTheSizeHintOncePerPass)
{
    auto evaluations = std::make_shared<int>(0);
    auto input = bq::signal::makeInput(avg::Vector2f(50.0f, 30.0f));

    auto hint = input.signal.map([evaluations](avg::Vector2f size) -> SizeHint
            {
                ++*evaluations;

                return simpleSizeHint(size.x(), size.y());
            });

    AnyWidget widget = makeWidget()
        | modifier::setSizeHint(std::move(hint))
        | modifier::handleGravity()
        ;

    auto instanceSignal = std::move(widget)(BuildParams())(
            bq::signal::constant(avg::Vector2f(100.0f, 100.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    int const afterInit = *evaluations;

    input.handle.set(avg::Vector2f(40.0f, 20.0f));
    context.update(nextFrame(1));

    EXPECT_EQ(afterInit + 1, *evaluations);

    Instance const& instance = context.evaluate<0>().get<0>();
    EXPECT_FLOAT_EQ(40.0f, instance.getSize()[0]);
    EXPECT_FLOAT_EQ(20.0f, instance.getSize()[1]);
}
