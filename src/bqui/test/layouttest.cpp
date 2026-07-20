#include <bqui/modifier/instancemodifier.h>
#include <bqui/modifier/setsizehint.h>
#include <bqui/modifier/widgetmodifier.h>

#include <bqui/widget/box.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/stack.h>
#include <bqui/widget/uniformgrid.h>
#include <bqui/widget/vbox.h>
#include <bqui/widget/widget.h>

#include <bqui/buildparams.h>
#include <bqui/dynamicbox.h>
#include <bqui/inputarea.h>
#include <bqui/simplesizehint.h>
#include <bqui/sizehint.h>

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
#include <optional>
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
    /**
     * @brief Registers a probe and builds the widget that carries it.
     */
    AnyWidget add(SizeHintResult width, SizeHintResult height);

    /**
     * @brief Builds another widget for the probe added at @p index.
     *
     * The new widget carries the same id and the same size hint as the
     * original one, so a container that is handed a freshly built child list
     * on every update still sees one continuous probe.
     */
    AnyWidget rebuild(size_t index) const;

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
        SizeHintResult width;
        SizeHintResult height;
    };

    std::vector<Probe> probes_;
};

AnyWidget tagGeometry(btl::UniqueId id)
{
    return makeWidget()
        | modifier::makeWidgetModifier(modifier::makeInstanceModifier(
                    [id](Instance instance)
                    {
                        auto areas = instance.getInputAreas();
                        areas.push_back(makeInputArea(id, instance.getObb()));

                        return std::move(instance)
                            .setInputAreas(std::move(areas));
                    }))
        ;
}

AnyWidget ProbeSet::add(SizeHintResult width, SizeHintResult height)
{
    probes_.push_back(Probe{ btl::makeUniqueId(), width, height });

    return rebuild(probes_.size() - 1);
}

AnyWidget ProbeSet::rebuild(size_t index) const
{
    Probe const& probe = probes_.at(index);

    return tagGeometry(probe.id)
        | modifier::setSizeHint(simpleSizeHint(probe.width, probe.height))
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
    result.reserve(probes_.size());

    for (auto const& probe : probes_)
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

SizeHintResult const fixed50 = {{ 50.0f, 50.0f, 50.0f }};

// Three hints that leave the minimums and the naturals satisfiable at a total
// of 150, so only part of the filler range is handed out and the layout has to
// distribute it: minimums total 60, naturals 100, fillers 200.
SizeHintResult const smallHint = {{ 20.0f, 40.0f, 40.0f }};
SizeHintResult const stretchyHint = {{ 10.0f, 30.0f, 130.0f }};
SizeHintResult const rigidHint = {{ 30.0f, 30.0f, 30.0f }};

// A hint whose minimum and natural are both zero and whose filler is far
// larger than any container used here, so a probe carrying it takes whatever
// it is offered on that axis.
SizeHintResult const fillHint = {{ 0.0f, 0.0f, 1000.0f }};

SizeHintResult const fixed100 = {{ 100.0f, 100.0f, 100.0f }};
SizeHintResult const fixed150 = {{ 150.0f, 150.0f, 150.0f }};

using KeyedWidgets = std::vector<std::pair<size_t, AnyWidget>>;

/**
 * @brief The child list dynamicBox takes, built from probe indices.
 *
 * Each probe is keyed by its own index, so a probe keeps its key across
 * additions, removals and reorderings.
 */
KeyedWidgets keyed(ProbeSet const& probes, std::vector<size_t> const& indices)
{
    KeyedWidgets result;
    for (size_t index : indices)
        result.push_back({ index, probes.rebuild(index) });

    return result;
}

bq::signal::FrameInfo nextFrame(uint64_t frameId)
{
    return bq::signal::FrameInfo(frameId, std::chrono::microseconds(0));
}

} // anonymous namespace

TEST(Layout, hboxDistributesFillerSpace)
{
    ProbeSet probes;

    std::vector<AnyWidget> children;
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

    std::vector<AnyWidget> children;
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

    std::vector<AnyWidget> children;
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

TEST(Layout, gravityCentersAChildInsideItsSlot)
{
    ProbeSet probes;

    std::vector<AnyWidget> children;
    children.push_back(probes.add(
                SizeHintResult{{ 20.0f, 50.0f, 50.0f }},
                SizeHintResult{{ 10.0f, 30.0f, 30.0f }}
                ));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(200.0f, 100.0f));

    ASSERT_EQ(1u, geometries.size());

    // The child cannot use more than 50x30, and the default gravity is
    // (0.5, 0.5), so it is centered in the 50x100 slot the hbox gave it.
    expectGeometry("centered", geometries[0], 0.0f, 35.0f, 50.0f, 30.0f);
}

TEST(Layout, hboxAggregatesChildSizeHints)
{
    ProbeSet probes;

    std::vector<AnyWidget> children;
    children.push_back(probes.add(smallHint, fixed50));
    children.push_back(probes.add(stretchyHint, fixed50));
    children.push_back(probes.add(rigidHint, fixed50));

    auto builder = hbox(std::move(children))(BuildParams());

    auto context = bq::signal::makeSignalContext(builder.getSizeHint());
    SizeHint const& hint = context.evaluate<0>().get<0>();

    SizeHintResult width = hint.getWidth();
    EXPECT_FLOAT_EQ(60.0f, width[0]);
    EXPECT_FLOAT_EQ(100.0f, width[1]);
    EXPECT_FLOAT_EQ(200.0f, width[2]);

    // Across the layout axis the hints are combined by taking the largest.
    SizeHintResult height = hint.getHeightForWidth(150.0f);
    EXPECT_FLOAT_EQ(50.0f, height[0]);
    EXPECT_FLOAT_EQ(50.0f, height[1]);
    EXPECT_FLOAT_EQ(50.0f, height[2]);
}

TEST(Layout, mapObbsPlacesChildrenLeftToRight)
{
    std::vector<SizeHint> hints {
        simpleSizeHint(smallHint, fixed50),
        simpleSizeHint(stretchyHint, fixed50),
        simpleSizeHint(rigidHint, fixed50)
    };

    auto obbs = mapObbs<Axis::x>(avg::Vector2f(150.0f, 50.0f), hints);

    ASSERT_EQ(3u, obbs.size());

    EXPECT_FLOAT_EQ(0.0f, obbs[0].getTransform().getTranslation()[0]);
    EXPECT_FLOAT_EQ(40.0f, obbs[0].getSize()[0]);
    EXPECT_FLOAT_EQ(40.0f, obbs[1].getTransform().getTranslation()[0]);
    EXPECT_FLOAT_EQ(80.0f, obbs[1].getSize()[0]);
    EXPECT_FLOAT_EQ(120.0f, obbs[2].getTransform().getTranslation()[0]);
    EXPECT_FLOAT_EQ(30.0f, obbs[2].getSize()[0]);
}

TEST(Layout, mapObbsPlacesChildrenTopToBottom)
{
    std::vector<SizeHint> hints {
        simpleSizeHint(fixed50, smallHint),
        simpleSizeHint(fixed50, stretchyHint),
        simpleSizeHint(fixed50, rigidHint)
    };

    auto obbs = mapObbs<Axis::y>(avg::Vector2f(50.0f, 150.0f), hints);

    ASSERT_EQ(3u, obbs.size());

    EXPECT_FLOAT_EQ(110.0f, obbs[0].getTransform().getTranslation()[1]);
    EXPECT_FLOAT_EQ(40.0f, obbs[0].getSize()[1]);
    EXPECT_FLOAT_EQ(30.0f, obbs[1].getTransform().getTranslation()[1]);
    EXPECT_FLOAT_EQ(80.0f, obbs[1].getSize()[1]);
    EXPECT_FLOAT_EQ(0.0f, obbs[2].getTransform().getTranslation()[1]);
    EXPECT_FLOAT_EQ(30.0f, obbs[2].getSize()[1]);
}

TEST(Layout, fixedChildrenKeepTheirSizeAtTheNaturalSize)
{
    ProbeSet probes;

    std::vector<AnyWidget> children;
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

    std::vector<AnyWidget> children;
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

TEST(Layout, hboxSquashesChildrenBelowTheirMinimum)
{
    ProbeSet probes;

    SizeHintResult const fixed40 = {{ 40.0f, 40.0f, 40.0f }};

    std::vector<AnyWidget> children;
    children.push_back(probes.add(fixed40, fixed50));
    children.push_back(probes.add(fixed40, fixed50));

    auto geometries = probes.realise(hbox(std::move(children)),
            avg::Vector2f(40.0f, 50.0f));

    ASSERT_EQ(2u, geometries.size());

    // Half of the requested minimum of 80 is available, so both children are
    // scaled to half of their minimum instead of overflowing the box.
    expectGeometry("first", geometries[0], 0.0f, 0.0f, 20.0f, 50.0f);
    expectGeometry("second", geometries[1], 20.0f, 0.0f, 20.0f, 50.0f);
}

TEST(Layout, hboxPlacesASingleStretchingChild)
{
    ProbeSet probes;

    std::vector<AnyWidget> children;
    children.push_back(probes.add(
                SizeHintResult{{ 10.0f, 20.0f, 100.0f }},
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

    SizeHintResult const zero = {{ 0.0f, 0.0f, 0.0f }};

    std::vector<AnyWidget> children;
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
    auto builder = hbox(std::vector<AnyWidget>())(BuildParams());

    auto sizeHint = builder.getSizeHint();
    auto instanceSignal = std::move(builder)(
            bq::signal::constant(avg::Vector2f(100.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(sizeHint),
            std::move(instanceSignal));

    SizeHint const& hint = context.evaluate<0>().get<0>();
    Instance const& instance = context.evaluate<1>().get<0>();

    SizeHintResult width = hint.getWidth();
    EXPECT_FLOAT_EQ(0.0f, width[0]);
    EXPECT_FLOAT_EQ(0.0f, width[1]);
    EXPECT_FLOAT_EQ(0.0f, width[2]);

    SizeHintResult height = hint.getHeightForWidth(100.0f);
    EXPECT_FLOAT_EQ(0.0f, height[0]);
    EXPECT_FLOAT_EQ(0.0f, height[1]);
    EXPECT_FLOAT_EQ(0.0f, height[2]);

    EXPECT_TRUE(instance.getInputAreas().empty());
}

// Pins current behaviour rather than asserting correctness. getSizes assumes
// the three entries of a hint are non-decreasing and never clamps its output
// against the size it was given; a hint whose natural size is below its minimum
// breaks that assumption and the children are handed more room than there is.
TEST(Layout, mapObbsOverflowsOnNonMonotonicHints)
{
    SizeHintResult const nonMonotonic = {{ 100.0f, 0.0f, 100.0f }};

    std::vector<SizeHint> hints {
        simpleSizeHint(nonMonotonic, fixed50),
        simpleSizeHint(nonMonotonic, fixed50)
    };

    auto obbs = mapObbs<Axis::x>(avg::Vector2f(200.0f, 50.0f), hints);

    ASSERT_EQ(2u, obbs.size());

    // Each child is given the whole 200, so the second one starts where the
    // container ends and the two together cover twice the container.
    EXPECT_FLOAT_EQ(0.0f, obbs[0].getTransform().getTranslation()[0]);
    EXPECT_FLOAT_EQ(200.0f, obbs[0].getSize()[0]);
    EXPECT_FLOAT_EQ(200.0f, obbs[1].getTransform().getTranslation()[0]);
    EXPECT_FLOAT_EQ(200.0f, obbs[1].getSize()[0]);
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
                SizeHintResult{{ 40.0f, 50.0f, 90.0f }},
                SizeHintResult{{ 10.0f, 30.0f, 30.0f }}
                ));
    children.push_back(probes.add(
                SizeHintResult{{ 10.0f, 80.0f, 30.0f }},
                SizeHintResult{{ 20.0f, 60.0f, 60.0f }}
                ));

    auto builder = stack(std::move(children))(BuildParams());

    auto context = bq::signal::makeSignalContext(builder.getSizeHint());
    SizeHint const& hint = context.evaluate<0>().get<0>();

    // The maximum is taken entry by entry, so the aggregate width matches
    // neither child.
    SizeHintResult width = hint.getWidth();
    EXPECT_FLOAT_EQ(40.0f, width[0]);
    EXPECT_FLOAT_EQ(80.0f, width[1]);
    EXPECT_FLOAT_EQ(90.0f, width[2]);

    SizeHintResult height = hint.getHeightForWidth(200.0f);
    EXPECT_FLOAT_EQ(20.0f, height[0]);
    EXPECT_FLOAT_EQ(60.0f, height[1]);
    EXPECT_FLOAT_EQ(60.0f, height[2]);
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

// Pins current behaviour rather than asserting correctness: the container hint
// scales the aggregate by the grid dimensions alone and never looks at a
// child's cell span. The child below spans the whole 2x2 grid and so receives
// the container's full size, yet the container asks for twice what the child
// wants on both axes.
TEST(Layout, uniformGridSizeHintIgnoresCellSpans)
{
    ProbeSet probes;

    AnyWidget grid = uniformGrid(2, 2)
        .cell(0, 0, 2, 2, probes.add(
                    SizeHintResult{{ 10.0f, 20.0f, 30.0f }},
                    SizeHintResult{{ 5.0f, 10.0f, 15.0f }}
                    ))
        ;

    auto builder = std::move(grid)(BuildParams());

    auto context = bq::signal::makeSignalContext(builder.getSizeHint());
    SizeHint const& hint = context.evaluate<0>().get<0>();

    SizeHintResult width = hint.getWidth();
    EXPECT_FLOAT_EQ(20.0f, width[0]);
    EXPECT_FLOAT_EQ(40.0f, width[1]);
    EXPECT_FLOAT_EQ(60.0f, width[2]);

    SizeHintResult height = hint.getHeightForWidth(40.0f);
    EXPECT_FLOAT_EQ(10.0f, height[0]);
    EXPECT_FLOAT_EQ(20.0f, height[1]);
    EXPECT_FLOAT_EQ(30.0f, height[2]);
}

TEST(Layout, nestedBoxesComposeTransforms)
{
    ProbeSet probes;

    std::vector<AnyWidget> row;
    row.push_back(probes.add(fixed50, fillHint));
    row.push_back(probes.add(fixed50, fillHint));

    std::vector<AnyWidget> column;
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

TEST(Layout, dynamicBoxPlacesChildrenLeftToRight)
{
    ProbeSet probes;

    KeyedWidgets children;
    children.push_back({ 0, probes.add(fixed100, fixed50) });
    children.push_back({ 1, probes.add(fixed50, fixed50) });

    auto input = bq::signal::makeInput(std::move(children));

    auto instanceSignal = dynamicBox<Axis::x>(input.signal)(BuildParams())(
            bq::signal::constant(avg::Vector2f(300.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    ASSERT_EQ(2u, geometries.size());

    expectGeometry("first", geometries[0], 0.0f, 0.0f, 100.0f, 50.0f);
    expectGeometry("second", geometries[1], 100.0f, 0.0f, 50.0f, 50.0f);
}

TEST(Layout, dynamicBoxPlacesAnAddedChild)
{
    ProbeSet probes;

    KeyedWidgets children;
    children.push_back({ 0, probes.add(fixed100, fixed50) });
    children.push_back({ 1, probes.add(fixed50, fixed50) });

    auto input = bq::signal::makeInput(std::move(children));

    auto instanceSignal = dynamicBox<Axis::x>(input.signal)(BuildParams())(
            bq::signal::constant(avg::Vector2f(300.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    KeyedWidgets grown = keyed(probes, { 0, 1 });
    grown.push_back({ 2, probes.add(fixed150, fixed50) });

    input.handle.set(std::move(grown));
    context.update(nextFrame(1));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    ASSERT_EQ(3u, geometries.size());

    expectGeometry("first", geometries[0], 0.0f, 0.0f, 100.0f, 50.0f);
    expectGeometry("second", geometries[1], 100.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("added", geometries[2], 150.0f, 0.0f, 150.0f, 50.0f);
}

TEST(Layout, dynamicBoxDropsARemovedChild)
{
    ProbeSet probes;

    KeyedWidgets children;
    children.push_back({ 0, probes.add(fixed100, fixed50) });
    children.push_back({ 1, probes.add(fixed50, fixed50) });
    children.push_back({ 2, probes.add(fixed150, fixed50) });

    auto input = bq::signal::makeInput(std::move(children));

    auto instanceSignal = dynamicBox<Axis::x>(input.signal)(BuildParams())(
            bq::signal::constant(avg::Vector2f(300.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    input.handle.set(keyed(probes, { 0, 2 }));
    context.update(nextFrame(1));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    ASSERT_EQ(3u, geometries.size());

    expectGeometry("first", geometries[0], 0.0f, 0.0f, 100.0f, 50.0f);
    expectNotRealised("removed", geometries[1]);
    expectGeometry("last", geometries[2], 100.0f, 0.0f, 150.0f, 50.0f);
}

TEST(Layout, dynamicBoxFollowsReorderedKeys)
{
    ProbeSet probes;

    KeyedWidgets children;
    children.push_back({ 0, probes.add(fixed100, fixed50) });
    children.push_back({ 1, probes.add(fixed50, fixed50) });
    children.push_back({ 2, probes.add(fixed150, fixed50) });

    auto input = bq::signal::makeInput(std::move(children));

    auto instanceSignal = dynamicBox<Axis::x>(input.signal)(BuildParams())(
            bq::signal::constant(avg::Vector2f(300.0f, 50.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    input.handle.set(keyed(probes, { 2, 0, 1 }));
    context.update(nextFrame(1));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    ASSERT_EQ(3u, geometries.size());

    // Every child keeps its key, so nothing is rebuilt and each one moves to
    // the slot its new position in the list earns it.
    expectGeometry("first", geometries[0], 150.0f, 0.0f, 100.0f, 50.0f);
    expectGeometry("second", geometries[1], 250.0f, 0.0f, 50.0f, 50.0f);
    expectGeometry("last", geometries[2], 0.0f, 0.0f, 150.0f, 50.0f);
}

// Pins a difference between dynamicBox and the layout() containers rather than
// asserting correctness: dynamicBox hands each child its slot directly, while
// layout() puts handleGravity() in front of every child. The probe below asks
// for 50x30 and gets the whole 50x100 slot; in an hbox it would be 50x30
// centered at y = 35.
TEST(Layout, dynamicBoxDoesNotApplyGravity)
{
    ProbeSet probes;

    KeyedWidgets children;
    children.push_back({ 0, probes.add(fixed50, rigidHint) });

    auto input = bq::signal::makeInput(std::move(children));

    auto instanceSignal = dynamicBox<Axis::x>(input.signal)(BuildParams())(
            bq::signal::constant(avg::Vector2f(300.0f, 100.0f)))
        .getInstance();

    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));

    auto geometries = probes.read(context.evaluate<0>().get<0>());

    ASSERT_EQ(1u, geometries.size());

    expectGeometry("only", geometries[0], 0.0f, 0.0f, 50.0f, 100.0f);
}
