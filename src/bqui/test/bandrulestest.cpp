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
// largest natural clamped into them, and it stretches if any child does).

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
        EXPECT_FLOAT_EQ(*expected.natural, *actual.natural);
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

AnyWidget rigid(avg::Vector2f size)
{
    return makeWidget() | modifier::fixedSize(size);
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

// A container adds nothing of its own, so one child free within its bounds or
// one filler passes through unchanged too.
TEST(BandRules, oneChildContainersAddNoDefault)
{
    auto freeChild = []()
    {
        return makeWidget()
            | modifier::minSize(avg::Vector2f(50.0f, 10.0f))
            | modifier::maxSize(avg::Vector2f(200.0f, 60.0f));
    };

    for (auto const& child : { freeChild(), filler() })
    {
        auto check = [&child](AnyWidget const& container)
        {
            expectSameBand(widthBandOf(child), widthBandOf(container));
            expectSameBand(heightBandOf(child), heightBandOf(container));
        };
        check(row({ child }));
        check(column({ child }));
        std::vector<AnyWidget> layers;
        layers.push_back(child);
        check(stack(std::move(layers)));
    }
}

// A label is rigid at its measured content: min, max and natural agree on both
// axes, so a fill() cannot stretch it.
TEST(BandRules, labelIsRigid)
{
    AnyWidget text = label(constant(std::string("rigid")));
    for (Constraints const& band : { widthBandOf(text), heightBandOf(text) })
    {
        ASSERT_TRUE(band.min && band.max && band.natural);
        EXPECT_GT(*band.natural, 0.0f);
        EXPECT_FLOAT_EQ(*band.natural, *band.min);
        EXPECT_FLOAT_EQ(*band.natural, *band.max);
    }

    btl::UniqueId const id = btl::makeUniqueId();
    Instance instance = realiseConverged(
            row({ withArea(label(constant(std::string("rigid")))
                    | modifier::fill(), id), filler() }),
            avg::Vector2f(400.0f, 100.0f));
    EXPECT_FLOAT_EQ(*widthBandOf(text).natural,
            readProbe(instance, id).size[0]);
    EXPECT_FLOAT_EQ(*heightBandOf(text).natural,
            readProbe(instance, id).size[1]);
}

// An hfiller's zero height is only a pull: in a row it neither caps the row's
// height nor holds a growing sibling back, and in a column it stays at zero
// height while it spans the column's width.
TEST(BandRules, hfillerPullsItsHeightToZero)
{
    btl::UniqueId const idTall = btl::makeUniqueId();
    AnyWidget tallRow = row({
            withArea(makeWidget()
                | modifier::defaultSize(avg::Vector2f(40.0f, 40.0f))
                | modifier::growHeight(), idTall),
            hfiller() });
    EXPECT_FALSE(heightBandOf(tallRow).max.has_value());

    Instance inRow = realiseConverged(std::move(tallRow),
            avg::Vector2f(200.0f, 300.0f));
    EXPECT_FLOAT_EQ(300.0f, readProbe(inRow, idTall).size[1]);

    btl::UniqueId const idFiller = btl::makeUniqueId();
    Instance inColumn = realiseConverged(
            column({ makeWidget() | modifier::defaultSize(
                        avg::Vector2f(40.0f, 40.0f)),
                withArea(hfiller(), idFiller),
                filler() }),
            avg::Vector2f(200.0f, 300.0f));
    Geometry spanned = readProbe(inColumn, idFiller);
    EXPECT_FLOAT_EQ(0.0f, spanned.size[1]);
    EXPECT_FLOAT_EQ(200.0f, spanned.size[0]);
}

// A column stretches across at the smallest weight of the rows that grow, and
// a rigid row caps it; its natural is the widest row's.
TEST(BandRules, columnStretchesAcrossWhenAnyRowDoes)
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
    ASSERT_TRUE(heldWidth.flex.has_value());
    EXPECT_FLOAT_EQ(1.0f, heldWidth.flex->coeff);
    ASSERT_TRUE(heldWidth.max.has_value());
    EXPECT_FLOAT_EQ(70.0f, *heldWidth.max);
    ASSERT_TRUE(heldWidth.natural.has_value());
    EXPECT_FLOAT_EQ(70.0f, *heldWidth.natural);
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
    EXPECT_FLOAT_EQ(100.0f, *width.natural);

    Instance instance = realiseConverged(
            row({ std::move(content), filler() }),
            avg::Vector2f(400.0f, 100.0f));

    Geometry capped = readProbe(instance, idCapped);
    EXPECT_FLOAT_EQ(40.0f, capped.size[0]);
    EXPECT_FLOAT_EQ(30.0f, capped.position[0]);
}

// A stack overlaps its children on both axes, so it takes the across rule on
// both: it stretches where any layer does, at the smallest weight, and is
// capped by its smallest max.
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
    EXPECT_FLOAT_EQ(80.0f, *width.natural);

    std::vector<AnyWidget> mixed;
    mixed.push_back(grower({ 50.0f, 30.0f }));
    mixed.push_back(rigid({ 80.0f, 20.0f }));
    AnyWidget held = stack(std::move(mixed));

    Constraints heldWidth = widthBandOf(held);
    ASSERT_TRUE(heldWidth.flex.has_value());
    ASSERT_TRUE(heldWidth.max.has_value());
    EXPECT_FLOAT_EQ(80.0f, *heldWidth.max);
    Constraints heldHeight = heightBandOf(held);
    ASSERT_TRUE(heldHeight.flex.has_value());
    ASSERT_TRUE(heldHeight.max.has_value());
    EXPECT_FLOAT_EQ(20.0f, *heldHeight.max);
}

// Each grid column takes the across rule over its cells and the grid the along
// rule over its columns: a column stretches if any of its cells does, and the
// grid's weight is the sum of its columns'.
TEST(BandRules, gridStretchesByItsColumnsIntersection)
{
    AnyWidget crossed = uniformGrid(2, 2)
        .cell(0, 0, 1, 1, grower({ 30.0f, 20.0f }))
        .cell(0, 1, 1, 1, rigid({ 50.0f, 20.0f }))
        .cell(1, 0, 1, 1, rigid({ 70.0f, 20.0f }))
        .cell(1, 1, 1, 1, grower({ 20.0f, 20.0f }));

    Constraints crossedWidth = widthBandOf(crossed);
    ASSERT_TRUE(crossedWidth.flex.has_value());
    EXPECT_FLOAT_EQ(2.0f, crossedWidth.flex->coeff);
    ASSERT_TRUE(heightBandOf(crossed).flex.has_value());
    ASSERT_TRUE(crossedWidth.natural.has_value());
    EXPECT_FLOAT_EQ(140.0f, *crossedWidth.natural);

    AnyWidget oneColumn = uniformGrid(2, 2)
        .cell(0, 0, 1, 1, grower({ 30.0f, 20.0f }))
        .cell(0, 1, 1, 1, grower({ 50.0f, 20.0f }))
        .cell(1, 0, 1, 1, rigid({ 70.0f, 20.0f }))
        .cell(1, 1, 1, 1, rigid({ 20.0f, 20.0f }));

    Constraints width = widthBandOf(oneColumn);
    ASSERT_TRUE(width.flex.has_value());
    EXPECT_FLOAT_EQ(1.0f, width.flex->coeff);
    Constraints height = heightBandOf(oneColumn);
    ASSERT_TRUE(height.flex.has_value());
    EXPECT_FLOAT_EQ(2.0f, height.flex->coeff);
}

// The adder shape: a column of {label, hfiller} rows. Beside a rigid leaf in a
// 400-wide row every ancestor stretches across, so the rows span the 300 the
// leaf leaves; a column holding a rigid 200 sibling is capped by it, and the
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
