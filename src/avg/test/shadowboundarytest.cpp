#include "builtinelements.h"

#include <avg/animated.h>
#include <avg/animationoptions.h>
#include <avg/brush.h>
#include <avg/builtineffects.h>
#include <avg/color.h>
#include <avg/curve/curves.h>
#include <avg/drawcontext.h>
#include <avg/drawing.h>
#include <avg/obb.h>
#include <avg/outlines.h>
#include <avg/painter.h>
#include <avg/rect.h>
#include <avg/region.h>
#include <avg/rendercounters.h>
#include <avg/rendertree.h>
#include <avg/shape.h>
#include <avg/shapefunction.h>
#include <avg/targetimage.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <ase/dummyplatform.h>
#include <ase/format.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>

#include <btl/runloop.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace
{

std::chrono::milliseconds const zero(0);

avg::Color const white(1.0f, 1.0f, 1.0f, 1.0f);
avg::Color const shade(0.0f, 0.0f, 0.0f, 0.5f);

using Element = avg::Drawing::Element;
using ShadowContent = avg::ShadowBoundaryNode::Content;

avg::Obb placed(float x, float y, float width, float height)
{
    return avg::Obb(avg::Vector2f(width, height), avg::translate(x, y));
}

avg::Shape box(avg::DrawContext const& context, avg::Vector2f size)
{
    return avg::Shape(context.pathBuilder()
            .start(0.0f, 0.0f)
            .lineTo(size[0], 0.0f)
            .lineTo(size[0], size[1])
            .lineTo(0.0f, size[1])
            .close()
            .build());
}

// A leaf that fills its area and casts it, as a bqui shape widget does.
std::shared_ptr<avg::RenderTreeNode> caster(avg::Obb const& obb)
{
    return avg::makeShapeNode(obb, std::nullopt,
            std::function<avg::Drawing(avg::DrawContext const&,
                avg::Vector2f)>(
                [](avg::DrawContext const& context, avg::Vector2f size)
                {
                    auto shape = box(context, size);
                    return avg::Shape(shape).fill(avg::Brush(white))
                        .addOutline(std::move(shape));
                }));
}

std::shared_ptr<avg::ContainerNode> container(
        std::vector<std::shared_ptr<avg::RenderTreeNode>> children)
{
    auto result = std::make_shared<avg::ContainerNode>(
            avg::Obb(avg::Vector2f(400.0f, 400.0f)),
            avg::ContainerNode::Stacking::flat);

    for (auto& child : children)
        result->addChild(std::move(child));

    return result;
}

avg::ShadowLight light(avg::Vector2f offset, float radius = 8.0f,
        avg::Color colour = shade)
{
    return avg::ShadowLight{
        avg::Animated<avg::Vector2f>(offset),
        avg::Animated<float>(radius),
        avg::Animated<avg::Color>(colour)
    };
}

std::shared_ptr<avg::ShadowBoundaryNode> boundary(
        std::shared_ptr<avg::RenderTreeNode> child,
        avg::ShadowLight shadowLight = light(avg::Vector2f(4.0f, -4.0f)),
        ShadowContent content = ShadowContent::flat)
{
    return std::make_shared<avg::ShadowBoundaryNode>(
            std::nullopt,
            std::move(shadowLight),
            content,
            std::move(child));
}

avg::Drawing drawNode(avg::RenderTreeNode const& node,
        std::chrono::milliseconds time = zero)
{
    avg::DrawContext context(pmr::new_delete_resource());

    return node.draw(context, avg::Obb(avg::Vector2f(400.0f, 400.0f)), time)
        .first;
}

// The effect elements nested around a shadow's fill, and that fill.
struct Stamp
{
    int blurPasses = 0;
    avg::Drawing::ShapeElement const* fill = nullptr;
};

Stamp stampOf(Element const& element)
{
    Stamp result;
    Element const* e = &element;

    while (auto const* effect = std::get_if<avg::Drawing::EffectElement>(e))
    {
        if (effect->inputs.size() != 1 || !effect->inputs[0].subDrawing
                || (*effect->inputs[0].subDrawing)->elements.size() != 1)
        {
            return Stamp();
        }

        ++result.blurPasses;
        e = &(*effect->inputs[0].subDrawing)->elements.front();
    }

    result.fill = std::get_if<avg::Drawing::ShapeElement>(e);
    return result;
}

avg::Rect filledBounds(avg::Shape const& shape)
{
    return shape.fillToRegion(pmr::new_delete_resource(),
            avg::Vector2f(1.0f, 1.0f)).getBoundingBox();
}

avg::Rect stampBounds(Element const& element)
{
    Stamp stamp = stampOf(element);
    if (!stamp.fill)
        return avg::Rect();

    return filledBounds(stamp.fill->shape);
}

void expectRect(avg::Rect const& r, float x, float y, float width,
        float height)
{
    EXPECT_NEAR(x, r.getLeft(), 0.01f);
    EXPECT_NEAR(y, r.getBottom(), 0.01f);
    EXPECT_NEAR(width, r.getWidth(), 0.01f);
    EXPECT_NEAR(height, r.getHeight(), 0.01f);
}

bool isFill(Element const& element)
{
    return std::holds_alternative<avg::Drawing::ShapeElement>(element);
}

avg::RenderCounters countersOf(avg::Drawing const& drawing,
        float scalingFactor, avg::Vector2i size)
{
    btl::RunLoop loop;
    ase::Platform platform = ase::makeDummyPlatform(loop);
    ase::RenderContext context = platform.makeRenderContext();
    avg::Painter painter(pmr::new_delete_resource(), context);
    avg::TargetImage target(context, size, ase::FORMAT_SRGBA);

    painter.clearImage(target);
    auto counters = painter.paintToImage(target, scalingFactor, drawing);
    painter.flush();

    return counters;
}

} // anonymous namespace

TEST(ShadowBoundary, oneCasterGetsOneBlurredFillUnderIt)
{
    auto d = drawNode(*boundary(caster(placed(10.0f, 20.0f, 100.0f, 50.0f))));

    ASSERT_EQ(2u, d.getElements().size());
    EXPECT_TRUE(isFill(d.getElements()[1]));

    Stamp stamp = stampOf(d.getElements()[0]);
    EXPECT_EQ(4, stamp.blurPasses);
    ASSERT_TRUE(stamp.fill);
    ASSERT_TRUE(stamp.fill->brush);
    EXPECT_EQ(shade, stamp.fill->brush->getColor());
    expectRect(filledBounds(stamp.fill->shape), 14.0f, 16.0f, 100.0f, 50.0f);
    EXPECT_TRUE(d.getOutlines().isEmpty());
}

TEST(ShadowBoundary, aNaNRadiusCastsAnUnblurredShadow)
{
    avg::DrawContext context(pmr::new_delete_resource());
    avg::Outlines outlines(pmr::new_delete_resource());
    outlines += box(context, avg::Vector2f(100.0f, 50.0f));

    avg::Drawing d = avg::shadowOf(pmr::new_delete_resource(),
            std::move(outlines), avg::Vector2f(4.0f, -4.0f),
            std::numeric_limits<float>::quiet_NaN(), shade);

    ASSERT_EQ(1u, d.getElements().size());
    EXPECT_TRUE(isFill(d.getElements()[0]));
}

TEST(ShadowBoundary, overlappingCastersShareOneFillAndOneBlur)
{
    auto d = drawNode(*boundary(container({
                    caster(placed(10.0f, 20.0f, 100.0f, 50.0f)),
                    caster(placed(60.0f, 40.0f, 100.0f, 50.0f))
                    })));

    ASSERT_EQ(3u, d.getElements().size());
    EXPECT_TRUE(isFill(d.getElements()[1]));
    EXPECT_TRUE(isFill(d.getElements()[2]));

    Stamp stamp = stampOf(d.getElements()[0]);
    EXPECT_EQ(4, stamp.blurPasses);
    ASSERT_TRUE(stamp.fill);
    expectRect(filledBounds(stamp.fill->shape), 14.0f, 16.0f, 150.0f, 70.0f);
}

TEST(ShadowBoundary, overlappingCastersRenderLikeOne)
{
    auto oneCaster = drawNode(*boundary(
                caster(placed(10.0f, 20.0f, 150.0f, 70.0f))));
    auto twoCasters = drawNode(*boundary(container({
                    caster(placed(10.0f, 20.0f, 100.0f, 50.0f)),
                    caster(placed(60.0f, 40.0f, 100.0f, 50.0f))
                    })));

    auto one = countersOf(oneCaster, 1.0f, avg::Vector2i(400, 400));
    auto two = countersOf(twoCasters, 1.0f, avg::Vector2i(400, 400));

    EXPECT_EQ(one.passes, two.passes);
    EXPECT_EQ(one.scratchTargets, two.scratchTargets);
    EXPECT_EQ(one.pixelsShaded, two.pixelsShaded);
}

TEST(ShadowBoundary, noCasterCastsNothing)
{
    auto plain = std::make_shared<avg::RectNode>(
            placed(10.0f, 20.0f, 100.0f, 50.0f), std::nullopt, 0.0f,
            std::make_optional(avg::Brush(white)), std::nullopt);

    auto d = drawNode(*boundary(plain));

    ASSERT_EQ(1u, d.getElements().size());
    EXPECT_TRUE(isFill(d.getElements()[0]));

    EXPECT_TRUE(drawNode(*boundary(nullptr)).getElements().empty());
}

TEST(ShadowBoundary, aTransparentLightCastsNothing)
{
    auto d = drawNode(*boundary(caster(placed(10.0f, 20.0f, 100.0f, 50.0f)),
                light(avg::Vector2f(4.0f, -4.0f), 8.0f,
                    avg::Color(0.0f, 0.0f, 0.0f, 0.0f))));

    EXPECT_EQ(1u, d.getElements().size());
    EXPECT_TRUE(d.getOutlines().isEmpty());
}

TEST(ShadowBoundary, aZeroRadiusIsAnUnblurredFill)
{
    auto d = drawNode(*boundary(caster(placed(10.0f, 20.0f, 100.0f, 50.0f)),
                light(avg::Vector2f(4.0f, -4.0f), 0.0f)));

    ASSERT_EQ(2u, d.getElements().size());
    Stamp stamp = stampOf(d.getElements()[0]);
    EXPECT_EQ(0, stamp.blurPasses);
    EXPECT_TRUE(stamp.fill);
}

TEST(ShadowBoundary, anOptedOutCasterCastsNothing)
{
    auto optedOut = std::make_shared<avg::OutlineNode>(
            placed(10.0f, 20.0f, 100.0f, 50.0f), std::nullopt,
            avg::OutlineNode::Mode::replace, std::nullopt,
            caster(placed(10.0f, 20.0f, 100.0f, 50.0f)));

    auto d = drawNode(*boundary(container({
                    optedOut,
                    caster(placed(200.0f, 20.0f, 100.0f, 50.0f))
                    })));

    ASSERT_EQ(3u, d.getElements().size());
    expectRect(stampBounds(d.getElements()[0]), 204.0f, 16.0f, 100.0f, 50.0f);
}

TEST(ShadowBoundary, glassCastsNothing)
{
    avg::FrostedGlassOptions options;
    options.radius = 8.0f;

    auto glass = std::make_shared<avg::SurfaceNode>(
            placed(10.0f, 20.0f, 100.0f, 50.0f), std::nullopt,
            avg::makeShapeFunction(&box),
            std::make_optional(avg::Brush(white)),
            std::make_optional(avg::Animated<avg::Brush>(
                    avg::Brush(avg::frostedGlass(options)))),
            nullptr);

    auto d = drawNode(*boundary(glass));

    // The glass and its fill, and no shadow under them
    ASSERT_EQ(2u, d.getElements().size());
    EXPECT_TRUE(isFill(d.getElements()[0]));
    EXPECT_TRUE(isFill(d.getElements()[1]));
}

TEST(ShadowBoundary, consumedOutlinesDoNotReachAnOuterBoundary)
{
    auto inner = boundary(caster(placed(10.0f, 20.0f, 100.0f, 50.0f)));
    auto outer = boundary(inner, light(avg::Vector2f(30.0f, -30.0f)));

    auto innerDrawing = drawNode(*inner);
    auto outerDrawing = drawNode(*outer);

    EXPECT_TRUE(innerDrawing.getOutlines().isEmpty());
    ASSERT_EQ(2u, outerDrawing.getElements().size());
    expectRect(stampBounds(outerDrawing.getElements()[0]),
            14.0f, 16.0f, 100.0f, 50.0f);
}

TEST(ShadowBoundary, nestedBoundariesEachCastTheirOwnUnderTheirContent)
{
    auto inner = boundary(caster(placed(10.0f, 20.0f, 100.0f, 50.0f)));
    auto outer = boundary(container({
                caster(placed(200.0f, 20.0f, 100.0f, 50.0f)),
                inner
                }), light(avg::Vector2f(10.0f, -10.0f)));

    auto d = drawNode(*outer);

    // The outer shadow and caster, then the inner boundary's shadow and
    // caster: the inner caster casts only through its own boundary.
    ASSERT_EQ(4u, d.getElements().size());
    expectRect(stampBounds(d.getElements()[0]), 210.0f, 10.0f, 100.0f, 50.0f);
    EXPECT_TRUE(isFill(d.getElements()[1]));
    expectRect(stampBounds(d.getElements()[2]), 14.0f, 16.0f, 100.0f, 50.0f);
    EXPECT_TRUE(isFill(d.getElements()[3]));
}

TEST(ShadowBoundary, scopedContentGoesInAScopeAfterTheShadow)
{
    auto d = drawNode(*boundary(caster(placed(10.0f, 20.0f, 100.0f, 50.0f)),
                light(avg::Vector2f(4.0f, -4.0f)), ShadowContent::scoped));

    ASSERT_EQ(2u, d.getElements().size());
    EXPECT_EQ(4, stampOf(d.getElements()[0]).blurPasses);
    EXPECT_TRUE(std::holds_alternative<avg::Drawing::BackdropScope>(
                d.getElements()[1]));
    EXPECT_TRUE(d.getOutlines().isEmpty());

    auto empty = drawNode(*boundary(nullptr, light(avg::Vector2f()),
                ShadowContent::scoped));
    EXPECT_TRUE(empty.getElements().empty());
}

TEST(ShadowBoundary, anOverflowingCasterStillCasts)
{
    auto d = drawNode(*boundary(caster(placed(500.0f, 20.0f, 50.0f, 50.0f)),
                light(avg::Vector2f(-4.0f, 0.0f))));

    ASSERT_EQ(2u, d.getElements().size());
    expectRect(stampBounds(d.getElements()[0]), 496.0f, 20.0f, 50.0f, 50.0f);
}

TEST(ShadowBoundary, theRendererCullsAShadowThatCannotReachTheTarget)
{
    // On a 400 wide target, the shadow of a caster at 404 starts at 400 and
    // its blur reaches back over the edge; at 420 the blur stays clear.
    auto reaching = drawNode(*boundary(
                caster(placed(404.0f, 20.0f, 50.0f, 50.0f)),
                light(avg::Vector2f(-4.0f, 0.0f))));
    auto clear = drawNode(*boundary(
                caster(placed(420.0f, 20.0f, 50.0f, 50.0f)),
                light(avg::Vector2f(-4.0f, 0.0f))));

    EXPECT_EQ(4u, countersOf(reaching, 1.0f,
                avg::Vector2i(400, 400)).passes);
    EXPECT_EQ(0u, countersOf(clear, 1.0f, avg::Vector2i(400, 400)).passes);
}

TEST(ShadowBoundary, theLightIsInLogicalUnits)
{
    auto d = drawNode(*boundary(caster(placed(10.0f, 20.0f, 100.0f, 50.0f)),
                light(avg::Vector2f(4.0f, -4.0f), 8.0f)));

    auto atOne = countersOf(d, 1.0f, avg::Vector2i(400, 400));
    auto atTwo = countersOf(d, 2.0f, avg::Vector2i(800, 800));

    // Offset and radius scale with the display: the same passes, each over
    // about four times the pixels.
    EXPECT_EQ(atOne.passes, atTwo.passes);
    EXPECT_EQ(atOne.draws, atTwo.draws);
    EXPECT_GT(static_cast<double>(atTwo.pixelsShaded),
            3.5 * static_cast<double>(atOne.pixelsShaded));
    EXPECT_LT(static_cast<double>(atTwo.pixelsShaded),
            4.5 * static_cast<double>(atOne.pixelsShaded));
}

TEST(ShadowBoundary, theLightAnimates)
{
    std::chrono::milliseconds const duration(1000);
    avg::AnimationOptions const options{ duration, avg::curve::linear };

    avg::RenderTree tree(boundary(caster(placed(10.0f, 20.0f, 100.0f, 50.0f)),
                light(avg::Vector2f(0.0f, 0.0f), 4.0f)));
    avg::RenderTree next(boundary(caster(placed(10.0f, 20.0f, 100.0f, 50.0f)),
                light(avg::Vector2f(20.0f, 0.0f), 12.0f)));

    auto [updated, nextUpdate] = std::move(tree).update(std::move(next),
            options, zero);

    auto const* node = dynamic_cast<avg::ShadowBoundaryNode const*>(
            updated.getRoot().get());
    ASSERT_TRUE(node);
    EXPECT_FLOAT_EQ(8.0f, node->getLight().radius.getValue(duration / 2));

    auto halfway = drawNode(*node, duration / 2);
    ASSERT_EQ(2u, halfway.getElements().size());
    expectRect(stampBounds(halfway.getElements()[0]),
            20.0f, 20.0f, 100.0f, 50.0f);

    avg::DrawContext context(pmr::new_delete_resource());
    avg::Obb const window(avg::Vector2f(400.0f, 400.0f));
    EXPECT_TRUE(updated.draw(context, window, duration / 2).second);
    EXPECT_FALSE(updated.draw(context, window, duration).second);
}

TEST(ShadowBoundary, anOldNodeOfAnotherKindIsNotPaired)
{
    avg::RenderTree tree(caster(placed(10.0f, 20.0f, 100.0f, 50.0f)));
    avg::RenderTree next(boundary(
                caster(placed(10.0f, 20.0f, 100.0f, 50.0f))));

    auto [updated, nextUpdate] = std::move(tree).update(std::move(next),
            avg::AnimationOptions{ std::chrono::milliseconds(100),
            avg::curve::linear }, zero);

    EXPECT_TRUE(dynamic_cast<avg::ShadowBoundaryNode const*>(
                updated.getRoot().get()));
}

TEST(ShadowBoundary, snapshotReportsTheChild)
{
    avg::DrawContext context(pmr::new_delete_resource());

    auto snapshot = boundary(caster(placed(10.0f, 20.0f, 100.0f, 50.0f)))
        ->snapshot(context, avg::Obb(avg::Vector2f(400.0f, 400.0f)), zero);

    EXPECT_EQ("ShadowBoundaryNode", snapshot.type);
    ASSERT_EQ(1u, snapshot.children.size());
}
