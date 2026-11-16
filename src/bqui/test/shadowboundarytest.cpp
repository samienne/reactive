#include <bqui/modifier/background.h>
#include <bqui/modifier/shadowboundary.h>
#include <bqui/modifier/surface.h>
#include <bqui/modifier/transform.h>

#include <bqui/shape/rectangle.h>

#include <bqui/widget/stack.h>
#include <bqui/widget/widget.h>

#include <bqui/buildparams.h>

#include <bq/signal/constant.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <avg/brush.h>
#include <avg/builtineffects.h>
#include <avg/color.h>
#include <avg/drawcontext.h>
#include <avg/drawing.h>
#include <avg/obb.h>
#include <avg/rect.h>
#include <avg/region.h>
#include <avg/rendertree.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <utility>
#include <variant>

using namespace bqui;
using namespace bqui::widget;

namespace
{

avg::Color const opaque(1.0f, 0.0f, 0.0f, 1.0f);
avg::Color const shade(0.0f, 0.0f, 0.0f, 0.5f);

avg::Vector2f const size(100.0f, 50.0f);
std::chrono::milliseconds const now(0);

Instance realise(AnyWidget widget)
{
    auto context = bq::signal::makeSignalContext(
            std::move(widget)(BuildParams())(
                bq::signal::constant(size)).getInstance());

    return context.evaluate<0>().get<0>();
}

avg::Drawing draw(AnyWidget widget)
{
    avg::DrawContext context(pmr::new_delete_resource());

    return realise(std::move(widget)).getRenderTree().draw(context,
            avg::Obb(size), now).first;
}

modifier::ShadowLight light()
{
    return modifier::ShadowLight{
        bq::signal::constant(avg::Vector2f(3.0f, -2.0f)),
        bq::signal::constant(6.0f),
        bq::signal::constant(shade)
    };
}

bool isEffect(avg::Drawing::Element const& element)
{
    return std::holds_alternative<avg::Drawing::EffectElement>(element);
}

AnyWidget glass()
{
    return makeWidget() | modifier::frostedGlass();
}

AnyWidget moved(AnyWidget widget, float x, float y)
{
    return std::move(widget) | modifier::transform(
            bq::signal::constant(avg::translate(x, y)));
}

// The bounds of the shadow's fill under the blur passes wrapping it
avg::Rect shadowBounds(avg::Drawing::Element const& element)
{
    avg::Drawing::Element const* e = &element;
    while (auto const* effect = std::get_if<avg::Drawing::EffectElement>(e))
    {
        if (effect->inputs.size() != 1 || !effect->inputs[0].subDrawing
                || (*effect->inputs[0].subDrawing)->elements.size() != 1)
        {
            return avg::Rect();
        }

        e = &(*effect->inputs[0].subDrawing)->elements.front();
    }

    auto const* fill = std::get_if<avg::Drawing::ShapeElement>(e);
    if (!fill)
        return avg::Rect();

    return fill->shape.fillToRegion(pmr::new_delete_resource(),
            avg::Vector2f(1.0f, 1.0f)).getBoundingBox();
}

void expectRect(avg::Rect const& r, float x, float y, float width,
        float height)
{
    EXPECT_NEAR(x, r.getLeft(), 0.01f);
    EXPECT_NEAR(y, r.getBottom(), 0.01f);
    EXPECT_NEAR(width, r.getWidth(), 0.01f);
    EXPECT_NEAR(height, r.getHeight(), 0.01f);
}

} // anonymous namespace

TEST(ShadowBoundary, yieldsAShadowBoundaryNodeWithTheLight)
{
    auto instance = realise(shape::rectangle().fill(opaque)
            | modifier::shadowBoundary(light()));

    auto const* node = dynamic_cast<avg::ShadowBoundaryNode const*>(
            instance.getRenderTree().getRoot().get());
    ASSERT_TRUE(node);

    EXPECT_EQ(avg::Vector2f(3.0f, -2.0f),
            node->getLight().offset.getValue(now));
    EXPECT_FLOAT_EQ(6.0f, node->getLight().radius.getValue(now));
    EXPECT_EQ(shade, node->getLight().colour.getValue(now));
    EXPECT_EQ(avg::ShadowBoundaryNode::Content::flat, node->getContent());
}

TEST(ShadowBoundary, anOpaqueShapeCastsUnderItself)
{
    auto d = draw(shape::rectangle().fill(opaque)
            | modifier::shadowBoundary(light()));

    ASSERT_EQ(2u, d.getElements().size());
    EXPECT_TRUE(isEffect(d.getElements()[0]));
    EXPECT_FALSE(isEffect(d.getElements()[1]));
    EXPECT_TRUE(d.getOutlines().isEmpty());
}

TEST(ShadowBoundary, anOpaqueCardCasts)
{
    auto d = draw(makeWidget()
            | modifier::surface(shape::rectangle(),
                bq::signal::AnySignal<avg::Brush>(
                    bq::signal::constant(avg::Brush(opaque))),
                std::nullopt)
            | modifier::shadowBoundary());

    ASSERT_EQ(2u, d.getElements().size());
    EXPECT_TRUE(isEffect(d.getElements()[0]));
}

TEST(ShadowBoundary, glassCastsNothing)
{
    auto d = draw(glass() | modifier::shadowBoundary(light()));

    ASSERT_EQ(1u, d.getElements().size());
    EXPECT_FALSE(isEffect(d.getElements()[0]));
}

TEST(ShadowBoundary, anOpaqueBackgroundCasts)
{
    auto d = draw(makeWidget()
            | modifier::background(bq::signal::constant(avg::Brush(opaque)))
            | modifier::shadowBoundary(light()));

    ASSERT_EQ(2u, d.getElements().size());
    expectRect(shadowBounds(d.getElements()[0]), 3.0f, -2.0f, 100.0f, 50.0f);
}

TEST(ShadowBoundary, theCombinedBoundaryScopesTheWidgetOverTheShadow)
{
    auto d = draw(widget::stack({
                shape::rectangle().fill(opaque),
                glass()
                })
            | modifier::shadowAndBackdropBoundary(light()));

    ASSERT_EQ(2u, d.getElements().size());
    EXPECT_TRUE(isEffect(d.getElements()[0]));
    EXPECT_TRUE(std::holds_alternative<avg::Drawing::BackdropScope>(
                d.getElements()[1]));
}

TEST(ShadowBoundary, contentOverflowingTheBoundaryCasts)
{
    auto d = draw(widget::stack({
                shape::rectangle().fill(opaque),
                moved(shape::rectangle().fill(opaque), 150.0f, 0.0f)
                })
            | modifier::shadowBoundary(light()));

    ASSERT_FALSE(d.getElements().empty());
    expectRect(shadowBounds(d.getElements()[0]), 3.0f, -2.0f, 250.0f, 50.0f);
}

TEST(ShadowBoundary, aMovedBoundaryCastsWhereItsContentIs)
{
    auto d = draw(moved(shape::rectangle().fill(opaque)
                | modifier::shadowBoundary(light()), 20.0f, 10.0f));

    ASSERT_FALSE(d.getElements().empty());
    expectRect(shadowBounds(d.getElements()[0]), 23.0f, 8.0f, 100.0f, 50.0f);
}

TEST(ShadowBoundary, anInnerBoundaryConsumesItsCasters)
{
    auto d = draw(shape::rectangle().fill(opaque)
            | modifier::shadowBoundary(light())
            | modifier::shadowBoundary(light()));

    EXPECT_EQ(2u, d.getElements().size());
}
