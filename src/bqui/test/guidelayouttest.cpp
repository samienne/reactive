#include "purelayouttestutil.h"

#include <bqui/modifier/alignguide.h>
#include <bqui/modifier/constraintsize.h>
#include <bqui/modifier/margin.h>
#include <bqui/modifier/setgravity.h>

#include <bqui/widget/filler.h>
#include <bqui/widget/guide.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/vbox.h>

#include <bq/signal/arraysignal.h>

#include <string>
#include <utility>
#include <vector>

using namespace bqui;
using namespace bqui::widget;
using namespace bq::signal;
using namespace bqui::test;

namespace
{

avg::Vector2f const window(400.0f, 300.0f);

ArraySignal<AnyWidget> list(std::vector<AnyWidget> widgets)
{
    std::vector<ArraySignal<AnyWidget>> items;
    for (auto& w : widgets)
        items.push_back(std::move(w));
    return ArraySignal<AnyWidget>(std::move(items));
}

// A leaf holding @p width x 20 at content strength.
AnyWidget leaf(btl::UniqueId id, float width)
{
    return probe(id, Band{ width, width, width }, Band{ 20.0f, 20.0f, 20.0f });
}

// A form row of a label and a 100-wide field, at the row's left edge.
AnyWidget formRow(AnyWidget label, btl::UniqueId fieldId)
{
    return hbox(list({ std::move(label), leaf(fieldId, 100.0f) }))
        | modifier::setGravity(constant(avg::Vector2f(0.0f, 0.5f)));
}

// Stacks @p rows from the top of the region.
AnyWidget column(std::vector<AnyWidget> rows)
{
    rows.push_back(vfiller());
    return vbox(list(std::move(rows)));
}

// A leaf publishing a baseline @p baseline below its top edge.
AnyWidget baselineLeaf(btl::UniqueId id, float height, float baseline)
{
    return withArea(makeWidget()
            | modifier::defaultSize(avg::Vector2f(30.0f, height))
            | modifier::makeWidgetModifier(modifier::makeBuilderModifier(
                    [baseline](AnyBuilder builder) -> AnyBuilder
                    {
                        setPureAnchor(builder, Axis::y, baselineAnchor,
                                constant(Anchor{ 0.0f, baseline }));
                        return builder;
                    })),
            id);
}

float left(Geometry const& g)
{
    return g.position[0];
}

float right(Geometry const& g)
{
    return g.position[0] + g.size[0];
}

float top(Geometry const& g)
{
    return window[1] - g.position[1] - g.size[1];
}

} // namespace

// Labels in separate rows bound to one guide by their right edges: the guide
// settles at the widest label, and the shorter label's row moves out to it, a
// placement yielding before a content size.
TEST(GuideLayout, formFieldsLineUpAtTheWidestLabel)
{
    XGuide labels;
    btl::UniqueId const label1 = btl::makeUniqueId();
    btl::UniqueId const label2 = btl::makeUniqueId();
    btl::UniqueId const field1 = btl::makeUniqueId();
    btl::UniqueId const field2 = btl::makeUniqueId();

    Instance instance = realiseConverged(column({
            formRow(leaf(label1, 50.0f) | modifier::alignRight(labels), field1),
            vbox(list({ formRow(
                    leaf(label2, 80.0f) | modifier::alignRight(labels),
                    field2) }))
                | modifier::setGravity(constant(avg::Vector2f(0.0f, 0.5f))) }),
            window);

    EXPECT_NEAR(80.0f, left(readProbe(instance, field1)), 0.01f);
    EXPECT_NEAR(80.0f, left(readProbe(instance, field2)), 0.01f);
    EXPECT_NEAR(80.0f, readProbe(instance, label2).size[0], 0.01f);
    EXPECT_NEAR(0.0f, left(readProbe(instance, label2)), 0.01f);
    EXPECT_NEAR(50.0f, readProbe(instance, label1).size[0], 0.01f);
    EXPECT_NEAR(80.0f, right(readProbe(instance, label1)), 0.01f);
}

// Baselines in sibling columns bound to one guide meet at the lower of their
// natural positions.
TEST(GuideLayout, baselinesAcrossColumnsShareAGuide)
{
    YGuide line;
    btl::UniqueId const a = btl::makeUniqueId();
    btl::UniqueId const b = btl::makeUniqueId();

    AnyWidget columns = hbox(list({
            vbox(list({ leaf(btl::makeUniqueId(), 30.0f),
                baselineLeaf(a, 40.0f, 30.0f) | modifier::alignBaseline(line),
                vfiller() })),
            vbox(list({ probe(btl::makeUniqueId(), Band{ 30, 30, 30 },
                        Band{ 50, 50, 50 }),
                baselineLeaf(b, 20.0f, 5.0f) | modifier::alignBaseline(line),
                vfiller() })) }));

    Instance instance = realiseConverged(std::move(columns), window);

    float baselineA = top(readProbe(instance, a)) + 30.0f;
    float baselineB = top(readProbe(instance, b)) + 5.0f;
    EXPECT_NEAR(55.0f, baselineA, 0.01f);
    EXPECT_NEAR(baselineA, baselineB, 0.01f);
}

// A binding made before a margin follows the inner box.
TEST(GuideLayout, marginKeepsTheBoundPoint)
{
    XGuide labels;
    btl::UniqueId const label1 = btl::makeUniqueId();
    btl::UniqueId const field1 = btl::makeUniqueId();
    btl::UniqueId const field2 = btl::makeUniqueId();

    Instance instance = realiseConverged(column({
            formRow(leaf(label1, 50.0f) | modifier::alignRight(labels)
                | modifier::margin(constant(10.0f)), field1),
            formRow(leaf(btl::makeUniqueId(), 80.0f)
                | modifier::alignRight(labels), field2) }),
            window);

    EXPECT_NEAR(80.0f, right(readProbe(instance, label1)), 0.01f);
    EXPECT_NEAR(90.0f, left(readProbe(instance, field1)), 0.01f);
    EXPECT_NEAR(80.0f, left(readProbe(instance, field2)), 0.01f);
}

// A size boundary's content is its own region: a guide bound inside it does
// not reach the bindings outside.
TEST(GuideLayout, guideDoesNotCrossASizeBoundary)
{
    XGuide labels;
    btl::UniqueId const field1 = btl::makeUniqueId();
    btl::UniqueId const field2 = btl::makeUniqueId();

    AnyWidget boundary = makeWidgetWithSize(
            [labels, field2](AnySignal<avg::Vector2f>)
            {
                return formRow(leaf(btl::makeUniqueId(), 80.0f)
                        | modifier::alignRight(labels), field2);
            })
        | modifier::fixedSize(constant(avg::Vector2f(200.0f, 20.0f)))
        | modifier::setGravity(constant(avg::Vector2f(0.0f, 0.5f)));

    Instance instance = realiseConverged(column({
            formRow(leaf(btl::makeUniqueId(), 50.0f)
                | modifier::alignRight(labels), field1),
            std::move(boundary) }),
            window);

    EXPECT_NEAR(50.0f, left(readProbe(instance, field1)), 0.01f);
    EXPECT_NEAR(80.0f, left(readProbe(instance, field2)), 0.01f);
}

// A guide bound once, or bound to an anchor the widget lacks, changes nothing.
TEST(GuideLayout, loneOrUnmatchedBindingIsANoOp)
{
    XGuide lone;
    YGuide line;
    btl::UniqueId const label1 = btl::makeUniqueId();
    btl::UniqueId const label2 = btl::makeUniqueId();
    btl::UniqueId const field2 = btl::makeUniqueId();

    Instance instance = realiseConverged(column({
            formRow(leaf(label1, 50.0f) | modifier::alignRight(lone),
                btl::makeUniqueId()),
            formRow(leaf(label2, 80.0f) | modifier::alignBaseline(line),
                field2) }),
            window);

    EXPECT_NEAR(50.0f, readProbe(instance, label1).size[0], 0.01f);
    EXPECT_NEAR(80.0f, left(readProbe(instance, field2)), 0.01f);
    EXPECT_NEAR(20.0f, top(readProbe(instance, label2)), 0.01f);
}

// A stated size beats the pull: a fixed label bound to one guide by both
// edges keeps its width, and the pull it leaves unmet is logged as a guide.
TEST(GuideLayout, fixedSizeBeatsTheGuideAndIsLogged)
{
    XGuide labels;
    btl::UniqueId const label = btl::makeUniqueId();
    btl::UniqueId const field = btl::makeUniqueId();

    testing::internal::CaptureStderr();
    Instance instance = realiseConverged(column({
            formRow(leaf(label, 50.0f) | modifier::fixedWidth(50.0f)
                | modifier::alignLeft(labels)
                | modifier::alignRight(labels), field) }),
            window);
    std::string log = testing::internal::GetCapturedStderr();

    Geometry g = readProbe(instance, label);
    EXPECT_NEAR(50.0f, g.size[0], 0.01f);
    EXPECT_NEAR(right(g), left(readProbe(instance, field)), 0.01f);
    EXPECT_NE(std::string::npos, log.find("unmet: guide")) << log;
    EXPECT_EQ(std::string::npos, log.find("infeasible")) << log;
}
