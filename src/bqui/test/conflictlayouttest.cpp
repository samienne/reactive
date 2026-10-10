#include "purelayouttestutil.h"

#include "widget/constraintlayout.h"

#include <bqui/modifier/buildermodifier.h>
#include <bqui/modifier/constraintsize.h>
#include <bqui/modifier/widgetmodifier.h>

#include <bqui/widget/hbox.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>

#include <arrange/expression.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

using namespace bqui;
using namespace bqui::widget;
using namespace bq::signal;
using namespace bqui::test;

namespace
{

Instance realiseRow(std::vector<ArraySignal<AnyWidget>> row,
        avg::Vector2f window)
{
    return realiseConverged(hbox(ArraySignal<AnyWidget>(std::move(row))),
            window);
}

// A 40x40 leaf carrying a plain `left == left` relation at arrange's default
// strength, which is required. As the leading child of the region's row it
// contradicts the tiling and the region anchor together.
AnyWidget requiredLeftLeaf(btl::UniqueId id, float left)
{
    return withArea(makeWidget()
            | modifier::defaultSize(avg::Vector2f(40.0f, 40.0f))
            | modifier::makeWidgetModifier(modifier::makeBuilderModifier(
                    [left](AnyBuilder builder) -> AnyBuilder
                    {
                        BoxVariables box = builder.getBoxVariables();
                        PureLayout old = builder.getPureLayout();

                        auto band = old.getWidth().map(
                                [box, left](Constraints const& c)
                                {
                                    Constraints out = c;
                                    out.relations.constraints.push_back(
                                            arrange::Expression(box.left)
                                            == arrange::Expression(
                                                static_cast<double>(left)));
                                    return out;
                                });

                        builder.setPureLayout(simplePureLayout(
                            AnySignal<Constraints>(std::move(band)),
                            [old](AnySignal<LayoutSolution> ws)
                            {
                                return old.getHeightForWidth(std::move(ws));
                            }));
                        return builder;
                    })),
            id);
}

} // namespace

// A max beats a fixed size, whichever is written last.
TEST(ConflictLayout, maxBeatsFixedSize)
{
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idA, fixed40, fixed40)
            | modifier::fixedWidth(300.0f) | modifier::maxWidth(200.0f));
    row.push_back(probe(idB, fixed40, fixed40)
            | modifier::maxWidth(200.0f) | modifier::fixedWidth(300.0f));

    Instance instance = realiseRow(std::move(row), { 800.0f, 100.0f });

    EXPECT_FLOAT_EQ(200.0f, readProbe(instance, idA).size[0]);
    EXPECT_FLOAT_EQ(200.0f, readProbe(instance, idB).size[0]);
    EXPECT_FLOAT_EQ(200.0f, readProbe(instance, idB).position[0]);
    EXPECT_FLOAT_EQ(40.0f, readProbe(instance, idA).size[1]);
}

// A min beats a fixed size.
TEST(ConflictLayout, minBeatsFixedSize)
{
    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(id, fixed40, fixed40)
            | modifier::minWidth(80.0f) | modifier::fixedWidth(50.0f));

    Instance instance = realiseRow(std::move(row), { 400.0f, 100.0f });

    EXPECT_FLOAT_EQ(80.0f, readProbe(instance, id).size[0]);
}

// A min beats a contradicting max, on a fixed leaf and on a flexing one.
TEST(ConflictLayout, minBeatsMax)
{
    btl::UniqueId const idFixed = btl::makeUniqueId();
    btl::UniqueId const idFlex = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idFixed, fixed40, fixed40)
            | modifier::minWidth(250.0f) | modifier::maxWidth(150.0f));
    row.push_back(fillerProbe(idFlex)
            | modifier::minWidth(120.0f) | modifier::maxWidth(60.0f));

    Instance instance = realiseRow(std::move(row), { 800.0f, 100.0f });

    EXPECT_FLOAT_EQ(250.0f, readProbe(instance, idFixed).size[0]);
    EXPECT_FLOAT_EQ(120.0f, readProbe(instance, idFlex).size[0]);
    EXPECT_FLOAT_EQ(250.0f, readProbe(instance, idFlex).position[0]);
}

// Fixed children wider than their fixed parent keep their sizes and overflow
// it; the parent keeps its own fixed size.
TEST(ConflictLayout, fixedChildrenOverflowFixedParent)
{
    btl::UniqueId const idParent = btl::makeUniqueId();
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();
    btl::UniqueId const idAfter = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> inner;
    inner.push_back(probe(idA, fixed40, fixed40) | modifier::fixedWidth(150.0f));
    inner.push_back(probe(idB, fixed40, fixed40) | modifier::fixedWidth(150.0f));

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(withArea(hbox(ArraySignal<AnyWidget>(std::move(inner)))
                | modifier::fixedWidth(200.0f), idParent));
    row.push_back(probe(idAfter, fixed40, fixed40));

    Instance instance = realiseRow(std::move(row), { 800.0f, 100.0f });

    EXPECT_FLOAT_EQ(200.0f, readProbe(instance, idParent).size[0]);
    EXPECT_FLOAT_EQ(150.0f, readProbe(instance, idA).size[0]);
    EXPECT_FLOAT_EQ(150.0f, readProbe(instance, idB).size[0]);
    EXPECT_FLOAT_EQ(150.0f, readProbe(instance, idB).position[0]);
    EXPECT_FLOAT_EQ(200.0f, readProbe(instance, idAfter).position[0]);
}

// The region anchor beats the root's own stated size.
TEST(ConflictLayout, regionAnchorBeatsRootSize)
{
    btl::UniqueId const id = btl::makeUniqueId();

    Instance instance = realiseConverged(probe(id, fixed40, fixed40)
            | modifier::fixedWidth(300.0f) | modifier::maxHeight(20.0f),
            { 400.0f, 100.0f });

    EXPECT_FLOAT_EQ(400.0f, readProbe(instance, id).size[0]);
    EXPECT_FLOAT_EQ(100.0f, readProbe(instance, id).size[1]);
}

// A required relation contradicting the region anchor made the solve
// infeasible while the anchor was required, collapsing every box in the region
// to zero. The anchor now yields, and the row lays out as usual.
TEST(ConflictLayout, requiredRelationAgainstAnchorKeepsRegion)
{
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(requiredLeftLeaf(idA, 30.0f));
    row.push_back(probe(idB, fixed40, fixed40));

    Instance instance = realiseRow(std::move(row), { 400.0f, 100.0f });

    EXPECT_FLOAT_EQ(40.0f, readProbe(instance, idA).size[0]);
    EXPECT_FLOAT_EQ(40.0f, readProbe(instance, idA).size[1]);
    EXPECT_FLOAT_EQ(40.0f, readProbe(instance, idB).size[0]);
    EXPECT_FLOAT_EQ(40.0f, readProbe(instance, idB).position[0]);
}

// A strong constraint the solution leaves violated is logged by kind.
TEST(ConflictLayout, unmetConstraintIsLogged)
{
    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(btl::makeUniqueId(), fixed40, fixed40)
            | modifier::fixedWidth(300.0f) | modifier::maxWidth(200.0f));

    testing::internal::CaptureStderr();
    realiseRow(std::move(row), { 800.0f, 100.0f });
    std::string log = testing::internal::GetCapturedStderr();

    EXPECT_NE(std::string::npos, log.find("unmet: fixed (200 == 300)"))
        << log;
}

// A layout with no conflict logs nothing.
TEST(ConflictLayout, consistentLayoutLogsNothing)
{
    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(btl::makeUniqueId(), fixed40, fixed40)
            | modifier::fixedWidth(100.0f) | modifier::maxWidth(200.0f));
    row.push_back(fillerProbe(btl::makeUniqueId()));

    testing::internal::CaptureStderr();
    realiseRow(std::move(row), { 800.0f, 100.0f });
    std::string log = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(log.empty()) << log;
}

// A flexing container's floor is an aggregated min, so it beats a fixed size
// on the container: fixed below its fixed content, the container takes the
// content's floor.
TEST(ConflictLayout, containerFloorBeatsFixedSize)
{
    btl::UniqueId const idParent = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> inner;
    inner.push_back(fillerProbe(btl::makeUniqueId()));
    inner.push_back(probe(btl::makeUniqueId(), fixed40, fixed40)
            | modifier::fixedWidth(80.0f));
    inner.push_back(probe(btl::makeUniqueId(), fixed40, fixed40)
            | modifier::fixedWidth(80.0f));

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(withArea(hbox(ArraySignal<AnyWidget>(std::move(inner)))
                | modifier::fixedWidth(100.0f), idParent));

    Instance instance = realiseRow(std::move(row), { 800.0f, 100.0f });

    EXPECT_FLOAT_EQ(160.0f, readProbe(instance, idParent).size[0]);
}
