#include <bqui/modifier/ondraw.h>
#include <bqui/modifier/outline.h>
#include <bqui/modifier/setsizehint.h>
#include <bqui/modifier/surface.h>
#include <bqui/modifier/transform.h>

#include <bqui/shape/circle.h>
#include <bqui/shape/rectangle.h>

#include <bqui/widget/hbox.h>
#include <bqui/widget/label.h>
#include <bqui/widget/widget.h>

#include <bqui/buildparams.h>
#include <bqui/simplesizehint.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <avg/animationoptions.h>
#include <avg/brush.h>
#include <avg/builtineffects.h>
#include <avg/color.h>
#include <avg/curve/curves.h>
#include <avg/drawcontext.h>
#include <avg/drawing.h>
#include <avg/obb.h>
#include <avg/outlines.h>
#include <avg/pen.h>
#include <avg/rect.h>
#include <avg/region.h>
#include <avg/rendertree.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace bqui;
using namespace bqui::widget;

namespace
{

avg::Color const opaque(1.0f, 0.0f, 0.0f, 1.0f);
avg::Color const translucent(1.0f, 0.0f, 0.0f, 0.3f);

avg::Vector2f const size(100.0f, 50.0f);
std::chrono::milliseconds const now(0);

avg::Rect rectAt(float x, float y, float width, float height)
{
    return avg::Rect(avg::Vector2f(x, y), avg::Vector2f(width, height));
}

Instance realise(AnyWidget widget, avg::Vector2f area = size)
{
    auto context = bq::signal::makeSignalContext(
            std::move(widget)(BuildParams())(
                bq::signal::constant(area)).getInstance());

    return context.evaluate<0>().get<0>();
}

avg::Drawing draw(AnyWidget widget)
{
    avg::DrawContext context(pmr::new_delete_resource());

    return realise(std::move(widget)).getRenderTree().getRoot()->draw(
            context, avg::Obb(size), now).first;
}

std::vector<avg::Rect> boundsOf(avg::Outlines const& outlines)
{
    std::vector<avg::Rect> result;

    for (auto const& shape : outlines.getShapes())
        result.push_back(shape.getControlBb());

    return result;
}

std::vector<avg::Rect> filledBoundsOf(avg::Outlines const& outlines)
{
    std::vector<avg::Rect> result;

    for (auto const& shape : outlines.getShapes())
    {
        result.push_back(shape.fillToRegion(pmr::new_delete_resource(),
                    avg::Vector2f(1.0f, 1.0f)).getBoundingBox());
    }

    return result;
}

std::vector<avg::Rect> drawnBounds(avg::Drawing const& drawing)
{
    std::vector<avg::Rect> result;

    for (auto const& element : drawing.getElements())
    {
        if (auto const* shape = std::get_if<avg::Drawing::ShapeElement>(
                    &element))
        {
            result.push_back(shape->shape.getControlBb());
        }
    }

    return result;
}

void expectSameBounds(std::vector<avg::Rect> const& expected,
        std::vector<avg::Rect> const& actual)
{
    ASSERT_EQ(expected.size(), actual.size());

    for (size_t i = 0; i < expected.size(); ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_NEAR(expected[i].getLeft(), actual[i].getLeft(), 0.01f);
        EXPECT_NEAR(expected[i].getBottom(), actual[i].getBottom(), 0.01f);
        EXPECT_NEAR(expected[i].getRight(), actual[i].getRight(), 0.01f);
        EXPECT_NEAR(expected[i].getTop(), actual[i].getTop(), 0.01f);
    }
}

AnyWidget sizedRectangle(float width)
{
    return shape::rectangle().fill(opaque)
        | modifier::setSizeHint(simpleSizeHint(width, 50.0f));
}

bool coversCenter(avg::Shape const& shape)
{
    auto region = shape.fillToRegion(pmr::new_delete_resource(),
            avg::Vector2f(1.0f, 1.0f));

    return !region.getClipped(rectAt(45.0f, 20.0f, 10.0f, 10.0f))
        .getBoundingBox().isEmpty();
}

avg::Drawing fillArea(avg::DrawContext const& context, avg::Vector2f size)
{
    return context.pathBuilder()
        .start(0.0f, 0.0f)
        .lineTo(size[0], 0.0f)
        .lineTo(size[0], size[1])
        .lineTo(0.0f, size[1])
        .close()
        .fill(avg::Brush(opaque));
}

} // anonymous namespace

TEST(Outline, opaqueFillSetsTheShape)
{
    auto result = draw(shape::rectangle().fill(opaque));

    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(0.0f, 0.0f, 100.0f, 50.0f) }),
            boundsOf(result.getOutlines()));
    ASSERT_EQ(1u, result.getElements().size());
    EXPECT_TRUE(coversCenter(result.getOutlines().getShapes()[0]));
}

TEST(Outline, translucentFillSetsNone)
{
    auto result = draw(shape::rectangle().fill(translucent));

    EXPECT_TRUE(result.getOutlines().isEmpty());
    EXPECT_EQ(1u, result.getElements().size());
}

TEST(Outline, fillAndStrokeIncludesTheStrokeWidth)
{
    auto result = draw(shape::rectangle().fillAndStroke(avg::Brush(opaque),
                avg::Pen(avg::Brush(translucent), 4.0f)));

    expectSameBounds((std::vector<avg::Rect>{
                rectAt(-4.0f, -4.0f, 108.0f, 58.0f) }),
            filledBoundsOf(result.getOutlines()));
    EXPECT_TRUE(coversCenter(result.getOutlines().getShapes()[0]));
}

TEST(Outline, strokeOnlyIsTheStrokeGeometry)
{
    auto result = draw(shape::rectangle().stroke(
                avg::Pen(avg::Brush(opaque), 4.0f)));

    expectSameBounds((std::vector<avg::Rect>{
                rectAt(-4.0f, -4.0f, 108.0f, 58.0f) }),
            filledBoundsOf(result.getOutlines()));
    EXPECT_FALSE(coversCenter(result.getOutlines().getShapes()[0]));
}

TEST(Outline, translucentStrokeOnlySetsNone)
{
    auto result = draw(shape::rectangle().stroke(
                avg::Pen(avg::Brush(translucent), 4.0f)));

    EXPECT_TRUE(result.getOutlines().isEmpty());
}

TEST(Outline, onDrawAndTextSetNone)
{
    auto drawn = draw(makeWidget() | modifier::onDraw(&fillArea));
    auto text = draw(label(bq::signal::constant(std::string("Hi"))));

    EXPECT_EQ(1u, drawn.getElements().size());
    EXPECT_TRUE(drawn.getOutlines().isEmpty());
    EXPECT_TRUE(text.getOutlines().isEmpty());
}

TEST(Outline, outlineOverrides)
{
    auto result = draw(shape::rectangle().fill(translucent)
            | modifier::outline(shape::circle()));

    ASSERT_EQ(1u, result.getOutlines().getShapes().size());
    EXPECT_TRUE(coversCenter(result.getOutlines().getShapes()[0]));
    EXPECT_EQ(1u, result.getElements().size());
}

TEST(Outline, addOutlineAdds)
{
    auto result = draw(shape::rectangle().fill(opaque)
            | modifier::addOutline(shape::circle()));

    EXPECT_EQ(2u, result.getOutlines().getShapes().size());
}

TEST(Outline, noOutlineRemoves)
{
    auto result = draw(shape::rectangle().fill(opaque)
            | modifier::noOutline());

    EXPECT_TRUE(result.getOutlines().isEmpty());
    EXPECT_EQ(1u, result.getElements().size());
}

TEST(Outline, frostedGlassSetsNone)
{
    auto result = draw(makeWidget() | modifier::frostedGlass());

    EXPECT_TRUE(result.getOutlines().isEmpty());
}

TEST(Outline, aPlacedWidgetsOutlineIsWhereItIsDrawn)
{
    std::vector<bq::signal::ArraySignal<AnyWidget>> children;
    children.push_back(sizedRectangle(100.0f));
    children.push_back(sizedRectangle(50.0f));

    avg::Vector2f const row(150.0f, 50.0f);
    avg::DrawContext context(pmr::new_delete_resource());

    auto result = realise(hbox(std::move(children)), row).getRenderTree()
        .getRoot()->draw(context, avg::Obb(row), now).first;

    EXPECT_EQ((std::vector<avg::Rect>{
                rectAt(0.0f, 0.0f, 100.0f, 50.0f),
                rectAt(100.0f, 0.0f, 50.0f, 50.0f)
                }), boundsOf(result.getOutlines()));
    expectSameBounds(drawnBounds(result), boundsOf(result.getOutlines()));
}

TEST(Outline, aTransformedWidgetsOutlineIsWhereItIsDrawn)
{
    auto result = draw(shape::rectangle().fill(opaque)
            | modifier::transform(bq::signal::constant(
                    avg::translate(30.0f, 10.0f) * avg::rotate(0.5f))));

    ASSERT_EQ(1u, result.getOutlines().getShapes().size());
    EXPECT_FALSE(rectAt(0.0f, 0.0f, 100.0f, 50.0f)
            == result.getOutlines().getControlBb());
    expectSameBounds(drawnBounds(result), boundsOf(result.getOutlines()));
}

TEST(Outline, theOutlineFollowsAnAnimatedPenWidth)
{
    std::chrono::milliseconds const duration(1000);
    avg::AnimationOptions const options{ duration, avg::curve::linear };

    auto stroked = [](float width)
    {
        return realise(shape::rectangle().stroke(
                    avg::Pen(avg::Brush(opaque), width))).getRenderTree();
    };

    auto [tree, nextUpdate] = stroked(4.0f).update(stroked(8.0f), options,
            now);

    avg::DrawContext context(pmr::new_delete_resource());
    auto halfway = tree.getRoot()->draw(context, avg::Obb(size),
            duration / 2).first;

    expectSameBounds((std::vector<avg::Rect>{
                rectAt(-6.0f, -6.0f, 112.0f, 62.0f) }),
            filledBoundsOf(halfway.getOutlines()));
}

TEST(Outline, translucentFillWithAnOpaquePenIsTheStrokeGeometry)
{
    auto result = draw(shape::rectangle().fillAndStroke(
                avg::Brush(translucent), avg::Pen(avg::Brush(opaque), 4.0f)));

    expectSameBounds((std::vector<avg::Rect>{
                rectAt(-4.0f, -4.0f, 108.0f, 58.0f) }),
            filledBoundsOf(result.getOutlines()));
    EXPECT_FALSE(coversCenter(result.getOutlines().getShapes()[0]));
}

TEST(Outline, anEffectFillSetsNone)
{
    auto result = draw(shape::rectangle().fill(
                avg::Brush(avg::tint(opaque, 0.5f))));

    EXPECT_TRUE(result.getOutlines().isEmpty());
    EXPECT_EQ(1u, result.getElements().size());
}

TEST(Outline, anOpaqueSurfaceSetsItsShape)
{
    auto result = draw(makeWidget() | modifier::surface(shape::rectangle(),
                bq::signal::AnySignal<avg::Brush>(
                    bq::signal::constant(avg::Brush(opaque))),
                std::nullopt));

    EXPECT_FALSE(result.getElements().empty());
    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(0.0f, 0.0f, 100.0f, 50.0f) }),
            boundsOf(result.getOutlines()));
}

TEST(Outline, aTranslucentSurfaceSetsNone)
{
    auto result = draw(makeWidget() | modifier::surface(shape::rectangle(),
                bq::signal::AnySignal<avg::Brush>(
                    bq::signal::constant(avg::Brush(translucent))),
                std::nullopt));

    EXPECT_TRUE(result.getOutlines().isEmpty());
}

TEST(Outline, anOpaqueGlassSurfaceSetsNone)
{
    auto result = draw(makeWidget() | modifier::surface(shape::rectangle(),
                bq::signal::AnySignal<avg::Brush>(
                    bq::signal::constant(avg::Brush(opaque))),
                bq::signal::AnySignal<avg::Effect>(bq::signal::constant(
                        avg::frostedGlass(avg::FrostedGlassOptions())))));

    EXPECT_TRUE(result.getOutlines().isEmpty());
}

TEST(Outline, aPlacedOverrideIsWhereTheWidgetIsDrawn)
{
    std::vector<bq::signal::ArraySignal<AnyWidget>> children;
    children.push_back(sizedRectangle(100.0f) | modifier::noOutline());
    children.push_back(shape::rectangle().fill(translucent)
            | modifier::outline(shape::rectangle())
            | modifier::setSizeHint(simpleSizeHint(50.0f, 50.0f)));

    avg::Vector2f const row(150.0f, 50.0f);
    avg::DrawContext context(pmr::new_delete_resource());

    auto result = realise(hbox(std::move(children)), row).getRenderTree()
        .getRoot()->draw(context, avg::Obb(row), now).first;

    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(100.0f, 0.0f, 50.0f, 50.0f) }),
            boundsOf(result.getOutlines()));
}
