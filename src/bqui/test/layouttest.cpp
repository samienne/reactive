#include <bqui/modifier/instancemodifier.h>
#include <bqui/modifier/setsizehint.h>
#include <bqui/modifier/widgetmodifier.h>

#include <bqui/widget/box.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/vbox.h>
#include <bqui/widget/widget.h>

#include <bqui/buildparams.h>
#include <bqui/inputarea.h>
#include <bqui/simplesizehint.h>
#include <bqui/sizehint.h>

#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <avg/obb.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <btl/uniqueid.h>

#include <gtest/gtest.h>

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
    AnyWidget add(SizeHintResult width, SizeHintResult height);

    std::vector<Geometry> realise(AnyWidget widget, avg::Vector2f size) const;

private:
    std::vector<btl::UniqueId> ids_;
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
    btl::UniqueId id = btl::makeUniqueId();
    ids_.push_back(id);

    return tagGeometry(id)
        | modifier::setSizeHint(simpleSizeHint(width, height))
        ;
}

std::vector<Geometry> ProbeSet::realise(AnyWidget widget,
        avg::Vector2f size) const
{
    auto instanceSignal = std::move(widget)(BuildParams())(
            bq::signal::constant(size))
        .getInstance();

    // The whole tree is evaluated through this one context. A signal evaluated
    // in a context of its own would be a different, parallel instantiation of
    // the same graph (bq::signal::SignalContext).
    auto context = bq::signal::makeSignalContext(std::move(instanceSignal));
    Instance const& instance = context.evaluate<0>().get<0>();

    std::vector<Geometry> result;
    result.reserve(ids_.size());

    for (auto const& id : ids_)
    {
        InputArea const* found = nullptr;
        for (auto const& area : instance.getInputAreas())
            if (area.getId() == id)
                found = &area;

        if (!found)
        {
            ADD_FAILURE() << "probe " << id << " was not realised";
            result.push_back(Geometry{});
            continue;
        }

        result.push_back(Geometry{
                found->getTransform().getTranslation(),
                found->getObbs().front().getSize()
                });
    }

    return result;
}

void expectGeometry(std::string const& label, Geometry const& geometry,
        float x, float y, float width, float height)
{
    SCOPED_TRACE(label);

    EXPECT_FLOAT_EQ(x, geometry.position[0]);
    EXPECT_FLOAT_EQ(y, geometry.position[1]);
    EXPECT_FLOAT_EQ(width, geometry.size[0]);
    EXPECT_FLOAT_EQ(height, geometry.size[1]);
}

SizeHintResult const fixed50 = {{ 50.0f, 50.0f, 50.0f }};

// Three hints that leave the minimums and the naturals satisfiable at a total
// of 150, so only part of the filler range is handed out and the layout has to
// distribute it: minimums total 60, naturals 100, fillers 200.
SizeHintResult const smallHint = {{ 20.0f, 40.0f, 40.0f }};
SizeHintResult const stretchyHint = {{ 10.0f, 30.0f, 130.0f }};
SizeHintResult const rigidHint = {{ 30.0f, 30.0f, 30.0f }};

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
