#include <bqui/modifier/handlegravity.h>
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
#include <bqui/inputarea.h>
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
    AnyWidget add(SizeHintResult width, SizeHintResult height);

    /**
     * @brief Registers a probe and returns its index.
     *
     * The index is what a dynamic child list carries, so that the widget can
     * be built from the index signal a forEach delegate is handed rather than
     * ahead of time.
     */
    size_t addIndexed(SizeHintResult width, SizeHintResult height);

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
        SizeHintResult width;
        SizeHintResult height;
    };

    // Held behind a pointer so that a copy taken by a delegate still sees a
    // probe the test registers after the tree was described.
    std::shared_ptr<std::vector<Probe>> probes_;
};

ProbeSet::ProbeSet() :
    probes_(std::make_shared<std::vector<Probe>>())
{
}

AnyWidget ProbeSet::add(SizeHintResult width, SizeHintResult height)
{
    return fromSignal(bq::signal::constant(addIndexed(width, height)));
}

size_t ProbeSet::addIndexed(SizeHintResult width, SizeHintResult height)
{
    probes_->push_back(Probe{ btl::makeUniqueId(), width, height });

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

                return simpleSizeHint(probe.width, probe.height);
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
    SizeHintResult getWidth() const
    {
        return fillHint;
    }

    SizeHintResult getHeightForWidth(float width) const
    {
        *queried = width;
        return {{ width, width, width }};
    }

    SizeHintResult getWidthForHeight(float height) const
    {
        *queried = height;
        return {{ height, height, height }};
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

TEST(Layout, gravityCentersAChildInsideItsSlot)
{
    ProbeSet probes;

    Children children;
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

    Children children;
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

TEST(Layout, hboxSquashesChildrenBelowTheirMinimum)
{
    ProbeSet probes;

    SizeHintResult const fixed40 = {{ 40.0f, 40.0f, 40.0f }};

    Children children;
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

    Children children;
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

// A hint whose natural size is below its minimum is read as if its natural
// were raised to the minimum, so the children still share the container
// between them rather than being handed more room than there is.
TEST(Layout, mapObbsFitsNonMonotonicHintsInTheContainer)
{
    SizeHintResult const nonMonotonic = {{ 100.0f, 0.0f, 100.0f }};

    std::vector<SizeHint> hints {
        simpleSizeHint(nonMonotonic, fixed50),
        simpleSizeHint(nonMonotonic, fixed50)
    };

    auto obbs = mapObbs<Axis::x>(avg::Vector2f(200.0f, 50.0f), hints);

    ASSERT_EQ(2u, obbs.size());

    EXPECT_FLOAT_EQ(0.0f, obbs[0].getTransform().getTranslation()[0]);
    EXPECT_FLOAT_EQ(100.0f, obbs[0].getSize()[0]);
    EXPECT_FLOAT_EQ(100.0f, obbs[1].getTransform().getTranslation()[0]);
    EXPECT_FLOAT_EQ(100.0f, obbs[1].getSize()[0]);
}

// The aggregate is read the way getSizes reads the children, so at its own
// natural width the box can give the second child its natural 50.
TEST(Layout, hboxAggregatesNonMonotonicHintsAsItAllocates)
{
    ProbeSet probes;

    Children children;
    children.push_back(probes.add(
                SizeHintResult{{ 100.0f, 0.0f, 100.0f }},
                fixed50
                ));
    children.push_back(probes.add(
                SizeHintResult{{ 0.0f, 50.0f, 50.0f }},
                fixed50
                ));

    auto builder = hbox(std::move(children))(BuildParams());

    auto context = bq::signal::makeSignalContext(builder.getSizeHint());
    SizeHint const& hint = context.evaluate<0>().get<0>();

    SizeHintResult width = hint.getWidth();
    EXPECT_FLOAT_EQ(100.0f, width[0]);
    EXPECT_FLOAT_EQ(150.0f, width[1]);
    EXPECT_FLOAT_EQ(150.0f, width[2]);

    std::vector<SizeHint> hints {
        simpleSizeHint(SizeHintResult{{ 100.0f, 0.0f, 100.0f }}, fixed50),
        simpleSizeHint(SizeHintResult{{ 0.0f, 50.0f, 50.0f }}, fixed50)
    };

    auto obbs = mapObbs<Axis::x>(avg::Vector2f(width[1], 50.0f), hints);

    ASSERT_EQ(2u, obbs.size());

    EXPECT_FLOAT_EQ(100.0f, obbs[0].getSize()[0]);
    EXPECT_FLOAT_EQ(50.0f, obbs[1].getSize()[0]);
}

// A maximum below the minimum does not pull a child below its minimum. The
// minimums below total exactly the size, so each child is given its minimum.
TEST(Layout, getSizesKeepsTheMinimumAboveASmallerMaximum)
{
    std::vector<std::array<float, 3>> hints {
        {{ 50.0f, 80.0f, 20.0f }},
        {{ 10.0f, 10.0f, 10.0f }}
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

// The child below spans the whole 2x2 grid and so receives the container's
// full size, so the container asks for exactly what the child wants.
TEST(Layout, uniformGridSizeHintAccountsForCellSpans)
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
    EXPECT_FLOAT_EQ(10.0f, width[0]);
    EXPECT_FLOAT_EQ(20.0f, width[1]);
    EXPECT_FLOAT_EQ(30.0f, width[2]);

    SizeHintResult height = hint.getHeightForWidth(40.0f);
    EXPECT_FLOAT_EQ(5.0f, height[0]);
    EXPECT_FLOAT_EQ(10.0f, height[1]);
    EXPECT_FLOAT_EQ(15.0f, height[2]);
}

// Each child's hint is spread over the cells it spans, and the grid needs the
// largest per-cell share on every cell. The row below wants 30 across two
// columns, 15 a column, which outweighs the 10 of the single cell under it.
TEST(Layout, uniformGridSizeHintTakesTheLargestShareOfACell)
{
    ProbeSet probes;

    AnyWidget grid = uniformGrid(2, 2)
        .cell(0, 0, 1, 1, probes.add(
                    SizeHintResult{{ 10.0f, 10.0f, 10.0f }},
                    SizeHintResult{{ 40.0f, 40.0f, 40.0f }}
                    ))
        .cell(0, 1, 2, 1, probes.add(
                    SizeHintResult{{ 30.0f, 30.0f, 30.0f }},
                    SizeHintResult{{ 10.0f, 10.0f, 10.0f }}
                    ))
        ;

    auto builder = std::move(grid)(BuildParams());

    auto context = bq::signal::makeSignalContext(builder.getSizeHint());
    SizeHint const& hint = context.evaluate<0>().get<0>();

    SizeHintResult width = hint.getWidth();
    EXPECT_FLOAT_EQ(30.0f, width[0]);
    EXPECT_FLOAT_EQ(30.0f, width[1]);
    EXPECT_FLOAT_EQ(30.0f, width[2]);

    // Rows are the other way round: the single cell's 40 outweighs the row's
    // 10, so each of the two rows needs 40.
    SizeHintResult height = hint.getHeightForWidth(30.0f);
    EXPECT_FLOAT_EQ(80.0f, height[0]);
    EXPECT_FLOAT_EQ(80.0f, height[1]);
    EXPECT_FLOAT_EQ(80.0f, height[2]);
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

    SizeHintResult height = hint.getHeightForWidth(60.0f);
    EXPECT_FLOAT_EQ(40.0f, *queried);
    EXPECT_FLOAT_EQ(60.0f, height[1]);

    SizeHintResult width = hint.getWidthForHeight(60.0f);
    EXPECT_FLOAT_EQ(40.0f, *queried);
    EXPECT_FLOAT_EQ(60.0f, width[1]);
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

// Adding or removing children does not rebuild the others. handleGravity()
// builds each child more than once, so the per-child count is measured from
// the first pass.
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
