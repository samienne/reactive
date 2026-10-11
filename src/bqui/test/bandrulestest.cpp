#include "purelayouttestutil.h"

#include "widget/constraintbox.h"

#include <bqui/modifier/constraintsize.h>

#include <bqui/widget/filler.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/label.h>
#include <bqui/widget/stack.h>
#include <bqui/widget/uniformgrid.h>
#include <bqui/widget/vbox.h>
#include <bqui/widget/widget.h>

#include <bqui/buildparams.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/signalcontext.h>

#include <avg/vector.h>

#include <btl/uniqueid.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

// A container aggregates its children's bands along its axis (sums, and it
// stretches if any child does) and across it (largest min, smallest max,
// largest natural clamped into them, and it stretches only if every child does).

using namespace bqui;
using namespace bqui::widget;
using namespace bq::signal;
using namespace bqui::test;

namespace
{

Constraints widthBandOf(AnyWidget const& widget)
{
    auto builder = widget.clone()(BuildParams());
    return makeSignalContext(builder.getPureLayout().getWidth())
        .evaluate<0>().get<0>();
}

Constraints heightBandOf(AnyWidget const& widget)
{
    auto builder = widget.clone()(BuildParams());
    return makeSignalContext(builder.getPureLayout().getHeightForWidth(
                constant(LayoutSolution()))).evaluate<0>().get<0>();
}

void expectSameBand(Constraints const& expected, Constraints const& actual)
{
    EXPECT_EQ(expected.min, actual.min);
    EXPECT_EQ(expected.max, actual.max);
    ASSERT_EQ(expected.natural.has_value(), actual.natural.has_value());
    if (expected.natural)
    {
        EXPECT_FLOAT_EQ(expected.natural->value, actual.natural->value);
        EXPECT_EQ(expected.natural->strength, actual.natural->strength);
    }
    ASSERT_EQ(expected.flex.has_value(), actual.flex.has_value());
    if (expected.flex)
        EXPECT_FLOAT_EQ(expected.flex->coeff, actual.flex->coeff);
}

// A leaf that grows on both axes at @p weight from a natural of @p natural.
AnyWidget grower(avg::Vector2f natural, float weight = 1.0f)
{
    return makeWidget()
        | modifier::defaultSize(natural)
        | modifier::grow(weight);
}

AnyWidget rigid(avg::Vector2f natural)
{
    return makeWidget() | modifier::defaultSize(natural);
}

std::vector<ArraySignal<AnyWidget>> arrays(std::vector<AnyWidget> widgets)
{
    std::vector<ArraySignal<AnyWidget>> result;
    for (auto& widget : widgets)
        result.push_back(std::move(widget));
    return result;
}

AnyWidget row(std::vector<AnyWidget> widgets)
{
    return hbox(ArraySignal<AnyWidget>(arrays(std::move(widgets))));
}

AnyWidget column(std::vector<AnyWidget> widgets)
{
    return vbox(ArraySignal<AnyWidget>(arrays(std::move(widgets))));
}

// The child the identity tests wrap: bounded on both axes, flexing only
// horizontally.
AnyWidget boundedChild()
{
    return makeWidget()
        | modifier::defaultSize(avg::Vector2f(50.0f, 30.0f))
        | modifier::minWidth(20.0f)
        | modifier::maxWidth(80.0f)
        | modifier::minHeight(10.0f)
        | modifier::maxHeight(60.0f)
        | modifier::growWidth();
}

void expectIdentity(AnyWidget const& container)
{
    AnyWidget child = boundedChild();
    expectSameBand(widthBandOf(child), widthBandOf(container));
    expectSameBand(heightBandOf(child), heightBandOf(container));
}

} // namespace

TEST(BandRules, hboxOfOneChildPublishesTheChildsBand)
{
    expectIdentity(row({ boundedChild() }));
}

TEST(BandRules, vboxOfOneChildPublishesTheChildsBand)
{
    expectIdentity(column({ boundedChild() }));
}

TEST(BandRules, stackOfOneChildPublishesTheChildsBand)
{
    std::vector<AnyWidget> children;
    children.push_back(boundedChild());
    expectIdentity(stack(std::move(children)));
}

TEST(BandRules, gridOfOneCellPublishesTheChildsBand)
{
    expectIdentity(uniformGrid(1, 1).cell(0, 0, 1, 1, boundedChild()));
}

// A column of filler rows stretches across at the smallest row weight; one rigid
// row keeps it from stretching, and its natural is then the widest row's.
TEST(BandRules, columnStretchesAcrossOnlyWhenEveryRowDoes)
{
    AnyWidget stretching = column({
            row({ rigid({ 30.0f, 20.0f }), grower({ 0.0f, 20.0f }, 2.0f) }),
            row({ rigid({ 50.0f, 20.0f }), grower({ 0.0f, 20.0f }, 3.0f) }),
            });
    Constraints width = widthBandOf(stretching);
    ASSERT_TRUE(width.flex.has_value());
    EXPECT_FLOAT_EQ(2.0f, width.flex->coeff);

    AnyWidget held = column({
            row({ rigid({ 30.0f, 20.0f }), hfiller() }),
            rigid({ 70.0f, 20.0f }),
            });
    Constraints heldWidth = widthBandOf(held);
    EXPECT_FALSE(heldWidth.flex.has_value());
    ASSERT_TRUE(heldWidth.natural.has_value());
    EXPECT_FLOAT_EQ(70.0f, heldWidth.natural->value);
}

// Across, the largest min beats the smallest max: the container's max is lifted
// to the min, its natural clamped up to it, and the capped child keeps its own
// size inside it, placed by its gravity.
TEST(BandRules, largestMinBeatsSmallestMaxAcross)
{
    btl::UniqueId const idCapped = btl::makeUniqueId();

    AnyWidget content = column({
            rigid({ 40.0f, 20.0f }) | modifier::minWidth(100.0f),
            probe(idCapped, { 40.0f, 40.0f, 60.0f }, fixed40),
            });

    Constraints width = widthBandOf(content);
    ASSERT_TRUE(width.min.has_value());
    ASSERT_TRUE(width.max.has_value());
    ASSERT_TRUE(width.natural.has_value());
    EXPECT_FLOAT_EQ(100.0f, *width.min);
    EXPECT_FLOAT_EQ(100.0f, *width.max);
    EXPECT_FLOAT_EQ(100.0f, width.natural->value);

    Instance instance = realiseConverged(
            row({ std::move(content), filler() }),
            avg::Vector2f(400.0f, 100.0f));

    Geometry capped = readProbe(instance, idCapped);
    EXPECT_FLOAT_EQ(40.0f, capped.size[0]);
    EXPECT_FLOAT_EQ(30.0f, capped.position[0]);
}

// A stack overlaps its children on both axes, so it takes the across rule on
// both: it stretches only where every layer does, at the smallest weight, and
// is capped by its smallest max.
TEST(BandRules, stackTakesTheAcrossRuleOnBothAxes)
{
    std::vector<AnyWidget> growing;
    growing.push_back(grower({ 50.0f, 30.0f }, 2.0f)
            | modifier::maxWidth(150.0f));
    growing.push_back(grower({ 80.0f, 20.0f }, 3.0f)
            | modifier::maxWidth(120.0f));
    AnyWidget stretching = stack(std::move(growing));

    Constraints width = widthBandOf(stretching);
    Constraints height = heightBandOf(stretching);
    ASSERT_TRUE(width.flex.has_value());
    ASSERT_TRUE(height.flex.has_value());
    EXPECT_FLOAT_EQ(2.0f, width.flex->coeff);
    EXPECT_FLOAT_EQ(2.0f, height.flex->coeff);
    ASSERT_TRUE(width.max.has_value());
    EXPECT_FLOAT_EQ(120.0f, *width.max);
    EXPECT_FALSE(height.max.has_value());
    ASSERT_TRUE(width.natural.has_value());
    EXPECT_FLOAT_EQ(80.0f, width.natural->value);

    std::vector<AnyWidget> mixed;
    mixed.push_back(grower({ 50.0f, 30.0f }));
    mixed.push_back(rigid({ 80.0f, 20.0f }));
    AnyWidget held = stack(std::move(mixed));

    EXPECT_FALSE(widthBandOf(held).flex.has_value());
    EXPECT_FALSE(heightBandOf(held).flex.has_value());
}

// Each grid column takes the across rule over its cells and the grid the along
// rule over its columns: a column with a rigid cell does not stretch, and the
// grid stretches while any column does.
TEST(BandRules, gridStretchesByItsColumnsIntersection)
{
    AnyWidget crossed = uniformGrid(2, 2)
        .cell(0, 0, 1, 1, grower({ 30.0f, 20.0f }))
        .cell(0, 1, 1, 1, rigid({ 50.0f, 20.0f }))
        .cell(1, 0, 1, 1, rigid({ 70.0f, 20.0f }))
        .cell(1, 1, 1, 1, grower({ 20.0f, 20.0f }));

    Constraints crossedWidth = widthBandOf(crossed);
    EXPECT_FALSE(crossedWidth.flex.has_value());
    EXPECT_FALSE(heightBandOf(crossed).flex.has_value());
    ASSERT_TRUE(crossedWidth.natural.has_value());
    EXPECT_FLOAT_EQ(140.0f, crossedWidth.natural->value);

    AnyWidget oneColumn = uniformGrid(2, 2)
        .cell(0, 0, 1, 1, grower({ 30.0f, 20.0f }))
        .cell(0, 1, 1, 1, grower({ 50.0f, 20.0f }))
        .cell(1, 0, 1, 1, rigid({ 70.0f, 20.0f }))
        .cell(1, 1, 1, 1, rigid({ 20.0f, 20.0f }));

    Constraints width = widthBandOf(oneColumn);
    ASSERT_TRUE(width.flex.has_value());
    EXPECT_FLOAT_EQ(1.0f, width.flex->coeff);
    EXPECT_FALSE(heightBandOf(oneColumn).flex.has_value());
}

// The adder shape: a column of {label, hfiller} rows. Beside a rigid leaf in a
// 400-wide row every ancestor stretches across, so the rows span the 300 the
// leaf leaves; a column holding a rigid 200 sibling stops the stretch, and the
// rows span its 200.
TEST(BandRules, filledRowsSpanUpToTheFirstRigidSibling)
{
    avg::Vector2f const window(400.0f, 300.0f);

    auto rows = [](btl::UniqueId idA, btl::UniqueId idB)
    {
        return column({
                withArea(row({ label(constant(std::string("a"))), hfiller() }),
                        idA),
                withArea(row({ label(constant(std::string("bb"))), hfiller() }),
                        idB),
                });
    };

    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();

    Instance spanning = realiseConverged(
            row({ column({ rows(idA, idB) }), rigid({ 100.0f, 40.0f }) }),
            window);

    EXPECT_FLOAT_EQ(300.0f, readProbe(spanning, idA).size[0]);
    EXPECT_FLOAT_EQ(300.0f, readProbe(spanning, idB).size[0]);
    EXPECT_FLOAT_EQ(0.0f, readProbe(spanning, idA).position[0]);

    btl::UniqueId const idC = btl::makeUniqueId();
    btl::UniqueId const idD = btl::makeUniqueId();

    Instance stopped = realiseConverged(
            row({ column({ rows(idC, idD), rigid({ 200.0f, 40.0f }) }),
                rigid({ 100.0f, 40.0f }) }),
            window);

    EXPECT_FLOAT_EQ(200.0f, readProbe(stopped, idC).size[0]);
    EXPECT_FLOAT_EQ(200.0f, readProbe(stopped, idD).size[0]);
}
