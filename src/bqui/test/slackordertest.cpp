#include "purelayouttestutil.h"

#include <bqui/modifier/constraintsize.h>

#include <bqui/widget/filler.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/widget.h>

#include <bqui/buildparams.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/input.h>
#include <bq/signal/signalcontext.h>

#include <avg/vector.h>

#include <btl/uniqueid.h>

#include <gtest/gtest.h>

#include <chrono>
#include <utility>
#include <vector>

// Along a row the slack goes first to a child free within its bounds, then to
// the growers by weight, and what they leave stays in the trailing gap. Short
// of space the order reverses: the growers give up their slack first, then a
// free child shrinks to its min, and a child holding a natural keeps it while
// the row overflows its end.

using namespace bqui;
using namespace bqui::widget;
using namespace bq::signal;
using namespace bqui::test;

namespace
{

AnyWidget row(std::vector<AnyWidget> widgets)
{
    std::vector<ArraySignal<AnyWidget>> children;
    for (auto& widget : widgets)
        children.push_back(std::move(widget));
    return hbox(ArraySignal<AnyWidget>(std::move(children)));
}

// A leaf with a min of 50 and a max of 200 and no natural.
AnyWidget free50to200(btl::UniqueId id)
{
    return withArea(makeWidget()
            | modifier::minWidth(50.0f)
            | modifier::maxWidth(200.0f), id);
}

AnyWidget naturalLeaf(btl::UniqueId id, float width)
{
    return withArea(makeWidget()
            | modifier::defaultSize(avg::Vector2f(width, 20.0f)), id);
}

float widthAt(AnyWidget widget, float width, btl::UniqueId id)
{
    Instance instance = realiseConverged(std::move(widget),
            avg::Vector2f(width, 100.0f));
    return readProbe(instance, id).size[0];
}

struct ThreeWidths
{
    float free;
    float natural;
    float filler;
};

struct FreeNaturalFiller
{
    btl::UniqueId free = btl::makeUniqueId();
    btl::UniqueId natural = btl::makeUniqueId();
    btl::UniqueId filler = btl::makeUniqueId();

    AnyWidget widget() const
    {
        return row({ free50to200(free), naturalLeaf(natural, 100.0f),
                fillerProbe(filler) });
    }

    ThreeWidths read(Instance const& instance) const
    {
        return ThreeWidths{
            readProbe(instance, free).size[0],
            readProbe(instance, natural).size[0],
            readProbe(instance, filler).size[0] };
    }

    ThreeWidths at(float width) const
    {
        return read(realiseConverged(widget(), avg::Vector2f(width, 100.0f)));
    }
};

void expectWidths(ThreeWidths expected, ThreeWidths actual)
{
    EXPECT_FLOAT_EQ(expected.free, actual.free);
    EXPECT_FLOAT_EQ(expected.natural, actual.natural);
    EXPECT_FLOAT_EQ(expected.filler, actual.filler);
}

} // namespace

TEST(SlackOrder, freeChildTakesSlackUpToItsMaxBeforeAFiller)
{
    btl::UniqueId const idFree = btl::makeUniqueId();
    btl::UniqueId const idFiller = btl::makeUniqueId();

    Instance instance = realiseConverged(
            row({ free50to200(idFree), fillerProbe(idFiller) }),
            avg::Vector2f(500.0f, 100.0f));

    EXPECT_FLOAT_EQ(200.0f, readProbe(instance, idFree).size[0]);
    EXPECT_FLOAT_EQ(300.0f, readProbe(instance, idFiller).size[0]);
}

TEST(SlackOrder, freeChildAloneTakesItsMax)
{
    btl::UniqueId const id = btl::makeUniqueId();
    EXPECT_FLOAT_EQ(200.0f, widthAt(row({ free50to200(id) }), 500.0f, id));
}

TEST(SlackOrder, naturalHoldsWhileTheFillerTakesTheRest)
{
    expectWidths({ 200.0f, 100.0f, 200.0f }, FreeNaturalFiller().at(500.0f));
}

// Squeezed, the filler empties first, then the free child shrinks to its min;
// past that the natural child keeps its natural and the row overflows.
TEST(SlackOrder, squeezeGivesUpFillerThenFreeChildThenNatural)
{
    FreeNaturalFiller const tree;

    expectWidths({ 200.0f, 100.0f, 100.0f }, tree.at(400.0f));
    expectWidths({ 200.0f, 100.0f, 0.0f }, tree.at(300.0f));
    expectWidths({ 120.0f, 100.0f, 0.0f }, tree.at(220.0f));
    expectWidths({ 50.0f, 100.0f, 0.0f }, tree.at(150.0f));
    expectWidths({ 50.0f, 100.0f, 0.0f }, tree.at(120.0f));
}

// The solve is repeated at each size, so a row taken to a squeezed size and
// back lands where it started.
TEST(SlackOrder, resolvingAtAnotherSizeAndBackRestoresTheLayout)
{
    FreeNaturalFiller const tree;
    auto size = makeInput(avg::Vector2f(500.0f, 100.0f));

    auto context = makeSignalContext(
            buildInRegion(tree.widget(), size.signal).getInstance());
    uint64_t frame = 0;
    auto step = [&](avg::Vector2f value)
    {
        size.handle.set(value);
        for (int i = 0; i < 3; ++i)
            context.update(FrameInfo(++frame, std::chrono::microseconds(0)));
        return tree.read(context.evaluate<0>().get<0>());
    };

    expectWidths({ 200.0f, 100.0f, 200.0f }, step({ 500.0f, 100.0f }));
    expectWidths({ 120.0f, 100.0f, 0.0f }, step({ 220.0f, 100.0f }));
    expectWidths({ 50.0f, 100.0f, 0.0f }, step({ 120.0f, 100.0f }));
    expectWidths({ 200.0f, 100.0f, 200.0f }, step({ 500.0f, 100.0f }));
}

TEST(SlackOrder, naturalsHoldAndTheGapTakesTheSlack)
{
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();

    Instance instance = realiseConverged(
            row({ naturalLeaf(idA, 60.0f), naturalLeaf(idB, 80.0f) }),
            avg::Vector2f(500.0f, 100.0f));

    EXPECT_FLOAT_EQ(60.0f, readProbe(instance, idA).size[0]);
    EXPECT_FLOAT_EQ(80.0f, readProbe(instance, idB).size[0]);
    EXPECT_FLOAT_EQ(60.0f, readProbe(instance, idB).position[0]);
}

TEST(SlackOrder, twoFillersShareEqually)
{
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idNatural = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();

    Instance instance = realiseConverged(
            row({ fillerProbe(idA), naturalLeaf(idNatural, 100.0f),
                fillerProbe(idB) }),
            avg::Vector2f(500.0f, 100.0f));

    EXPECT_FLOAT_EQ(200.0f, readProbe(instance, idA).size[0]);
    EXPECT_FLOAT_EQ(100.0f, readProbe(instance, idNatural).size[0]);
    EXPECT_FLOAT_EQ(200.0f, readProbe(instance, idB).size[0]);
}

// A fill() grows from its natural and a filler from zero, sharing the slack by
// weight.
TEST(SlackOrder, fillAndFillerShareByWeight)
{
    btl::UniqueId const idFill = btl::makeUniqueId();
    btl::UniqueId const idFiller = btl::makeUniqueId();

    Instance even = realiseConverged(
            row({ naturalLeaf(idFill, 100.0f) | modifier::fill(),
                fillerProbe(idFiller) }),
            avg::Vector2f(500.0f, 100.0f));

    EXPECT_FLOAT_EQ(300.0f, readProbe(even, idFill).size[0]);
    EXPECT_FLOAT_EQ(200.0f, readProbe(even, idFiller).size[0]);

    Instance weighted = realiseConverged(
            row({ naturalLeaf(idFill, 50.0f) | modifier::grow(2.0f),
                fillerProbe(idFiller) }),
            avg::Vector2f(350.0f, 100.0f));

    EXPECT_FLOAT_EQ(250.0f, readProbe(weighted, idFill).size[0]);
    EXPECT_FLOAT_EQ(100.0f, readProbe(weighted, idFiller).size[0]);
}
