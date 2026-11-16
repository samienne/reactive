#include <avg/animated.h>
#include <avg/animationoptions.h>
#include <avg/brush.h>
#include <avg/color.h>
#include <avg/curve/curves.h>
#include <avg/drawcontext.h>
#include <avg/drawing.h>
#include <avg/obb.h>
#include <avg/outlines.h>
#include <avg/rect.h>
#include <avg/region.h>
#include <avg/rendertree.h>
#include <avg/shape.h>
#include <avg/shapefunction.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <pmr/new_delete_resource.h>

#include "builtinelements.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace
{

std::chrono::milliseconds const zero(0);

avg::Color const white(1.0f, 1.0f, 1.0f, 1.0f);

avg::Obb placed(float x, float y, float width, float height)
{
    return avg::Obb(avg::Vector2f(width, height), avg::translate(x, y));
}

avg::Rect rectAt(float x, float y, float width, float height)
{
    return avg::Rect(avg::Vector2f(x, y), avg::Vector2f(width, height));
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

avg::Shape boxAt(float x, float y, float width, float height)
{
    avg::DrawContext context(pmr::new_delete_resource());

    return box(context, avg::Vector2f(width, height))
        .translate(avg::Vector2f(x, y));
}

avg::Drawing filledBoxAt(float x, float y, float width, float height)
{
    return boxAt(x, y, width, height).fill(avg::Brush(white));
}

avg::Rect filledBounds(avg::Shape const& shape)
{
    return shape.fillToRegion(pmr::new_delete_resource(),
            avg::Vector2f(1.0f, 1.0f)).getBoundingBox();
}

std::vector<avg::Rect> boundsOf(avg::Outlines const& outlines)
{
    std::vector<avg::Rect> result;

    for (auto const& shape : outlines.getShapes())
        result.push_back(shape.getControlBb());

    return result;
}

avg::Shape square(avg::DrawContext const& context, avg::Vector2f,
        float side)
{
    return box(context, avg::Vector2f(side, side));
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

std::shared_ptr<avg::RectNode> rect(avg::Obb const& obb,
        std::optional<avg::AnimationOptions> options = std::nullopt)
{
    return std::make_shared<avg::RectNode>(
            obb,
            std::move(options),
            0.0f,
            std::make_optional(avg::Brush(white)),
            std::nullopt
            );
}

std::shared_ptr<avg::OutlineNode> outlined(avg::Obb const& area,
        std::shared_ptr<avg::RenderTreeNode> child,
        avg::OutlineNode::Mode mode = avg::OutlineNode::Mode::replace,
        std::optional<avg::ShapeFunction> outline
            = avg::makeShapeFunction(&box),
        std::optional<avg::AnimationOptions> options = std::nullopt)
{
    return std::make_shared<avg::OutlineNode>(
            area,
            std::move(options),
            mode,
            std::move(outline),
            std::move(child)
            );
}

std::shared_ptr<avg::ContainerNode> container(avg::Obb const& obb,
        std::vector<std::shared_ptr<avg::RenderTreeNode>> children,
        avg::ContainerNode::Stacking stacking
            = avg::ContainerNode::Stacking::flat)
{
    auto result = std::make_shared<avg::ContainerNode>(obb, stacking);

    for (auto& child : children)
        result->addChild(std::move(child));

    return result;
}

std::pair<avg::Drawing, bool> drawNode(avg::RenderTreeNode const& node,
        std::chrono::milliseconds time = zero)
{
    avg::DrawContext context(pmr::new_delete_resource());

    return node.draw(context, avg::Obb(avg::Vector2f(400.0f, 400.0f)), time);
}

avg::Outlines outlinesOf(avg::RenderTreeNode const& node,
        std::chrono::milliseconds time = zero)
{
    return drawNode(node, time).first.getOutlines();
}

} // anonymous namespace

TEST(Outlines, transformMovesEveryShape)
{
    avg::Outlines outlines(pmr::new_delete_resource(),
            boxAt(0.0f, 0.0f, 10.0f, 10.0f));
    outlines += boxAt(20.0f, 0.0f, 10.0f, 10.0f);

    auto moved = std::move(outlines).transform(avg::translate(5.0f, 7.0f));

    EXPECT_EQ((std::vector<avg::Rect>{
                rectAt(5.0f, 7.0f, 10.0f, 10.0f),
                rectAt(25.0f, 7.0f, 10.0f, 10.0f)
                }), boundsOf(moved));
    EXPECT_EQ(rectAt(5.0f, 7.0f, 30.0f, 10.0f), moved.getControlBb());
}

TEST(Outlines, clipKeepsInsideDropsOutsideAndCutsTheRest)
{
    avg::Outlines outlines(pmr::new_delete_resource());
    outlines += boxAt(10.0f, 10.0f, 10.0f, 10.0f);
    outlines += boxAt(200.0f, 10.0f, 10.0f, 10.0f);
    outlines += boxAt(90.0f, 10.0f, 20.0f, 10.0f);

    auto clipped = std::move(outlines).clip(
            rectAt(0.0f, 0.0f, 100.0f, 100.0f));

    ASSERT_EQ(2u, clipped.getShapes().size());
    EXPECT_EQ(rectAt(10.0f, 10.0f, 10.0f, 10.0f),
            clipped.getShapes()[0].getControlBb());

    auto cut = filledBounds(clipped.getShapes()[1]);
    EXPECT_NEAR(90.0f, cut.getLeft(), 0.5f);
    EXPECT_NEAR(100.0f, cut.getRight(), 0.5f);
}

TEST(Outlines, aZeroAreaClipDropsThemAll)
{
    avg::Outlines outlines(pmr::new_delete_resource(),
            boxAt(0.0f, 0.0f, 10.0f, 10.0f));

    EXPECT_TRUE(avg::Outlines(outlines).clip(
                rectAt(5.0f, 0.0f, 0.0f, 10.0f)).isEmpty());
    EXPECT_TRUE(filledBoxAt(0.0f, 0.0f, 10.0f, 10.0f)
            .addOutline(boxAt(0.0f, 0.0f, 10.0f, 10.0f))
            .clip(avg::Obb(avg::Vector2f(10.0f, 0.0f)))
            .getOutlines().isEmpty());
}

TEST(Outlines, appendingToItselfDoublesThem)
{
    avg::Outlines outlines(pmr::new_delete_resource(),
            boxAt(0.0f, 0.0f, 10.0f, 10.0f));
    outlines += boxAt(20.0f, 0.0f, 10.0f, 10.0f);

    outlines += outlines;

    EXPECT_EQ((std::vector<avg::Rect>{
                rectAt(0.0f, 0.0f, 10.0f, 10.0f),
                rectAt(20.0f, 0.0f, 10.0f, 10.0f),
                rectAt(0.0f, 0.0f, 10.0f, 10.0f),
                rectAt(20.0f, 0.0f, 10.0f, 10.0f)
                }), boundsOf(outlines));

    auto d = filledBoxAt(0.0f, 0.0f, 10.0f, 10.0f)
        .addOutline(boxAt(0.0f, 0.0f, 10.0f, 10.0f));

    d += d;

    EXPECT_EQ(2u, d.getOutlines().getShapes().size());
    EXPECT_EQ(2u, d.getElements().size());
}

TEST(Outlines, filterByRectDropsWhatDoesNotOverlap)
{
    avg::Outlines outlines(pmr::new_delete_resource());
    outlines += boxAt(10.0f, 10.0f, 10.0f, 10.0f);
    outlines += boxAt(200.0f, 10.0f, 10.0f, 10.0f);

    auto kept = std::move(outlines).filterByRect(
            rectAt(0.0f, 0.0f, 100.0f, 100.0f));

    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(10.0f, 10.0f, 10.0f, 10.0f) }),
            boundsOf(kept));
}

TEST(DrawingOutlines, areNotElementsAndDoNotWidenTheBounds)
{
    auto d = filledBoxAt(0.0f, 0.0f, 10.0f, 10.0f)
        .addOutline(boxAt(0.0f, 0.0f, 50.0f, 50.0f));

    EXPECT_EQ(1u, d.getElements().size());
    EXPECT_EQ(rectAt(0.0f, 0.0f, 10.0f, 10.0f), d.getControlBb());
    EXPECT_EQ(1u, d.getOutlines().getShapes().size());

    auto taken = d.takeOutlines();

    EXPECT_EQ(1u, taken.getShapes().size());
    EXPECT_TRUE(d.getOutlines().isEmpty());

    d.setOutlines(std::move(taken));
    EXPECT_TRUE(d.clearOutlines().getOutlines().isEmpty());

    d.addOutline(boxAt(0.0f, 0.0f, 50.0f, 50.0f));
    EXPECT_TRUE(std::move(d).clearOutlines().getOutlines().isEmpty());
}

TEST(DrawingOutlines, transformAndScaleMoveThemWithTheElements)
{
    auto d = (filledBoxAt(0.0f, 0.0f, 10.0f, 10.0f)
        .addOutline(boxAt(0.0f, 0.0f, 10.0f, 10.0f))
        * 2.0f)
        .translate(avg::Vector2f(5.0f, 0.0f));

    EXPECT_EQ(rectAt(5.0f, 0.0f, 20.0f, 20.0f), d.getControlBb());
    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(5.0f, 0.0f, 20.0f, 20.0f) }),
            boundsOf(d.getOutlines()));
}

TEST(DrawingOutlines, clipCutsThemEvenWhenTheElementsFit)
{
    auto d = filledBoxAt(10.0f, 10.0f, 10.0f, 10.0f)
        .addOutline(boxAt(10.0f, 10.0f, 10.0f, 10.0f))
        .addOutline(boxAt(90.0f, 10.0f, 20.0f, 10.0f))
        .addOutline(boxAt(200.0f, 10.0f, 10.0f, 10.0f))
        .clip(rectAt(0.0f, 0.0f, 100.0f, 100.0f));

    ASSERT_EQ(2u, d.getOutlines().getShapes().size());
    EXPECT_EQ(rectAt(10.0f, 10.0f, 10.0f, 10.0f),
            d.getOutlines().getShapes()[0].getControlBb());
    EXPECT_NEAR(100.0f,
            filledBounds(d.getOutlines().getShapes()[1]).getRight(), 0.5f);
}

TEST(DrawingOutlines, aRotatedClipCutsThemInItsOwnSpace)
{
    auto toWorld = avg::translate(200.0f, 0.0f) * avg::rotate(0.7854f);

    auto d = (toWorld * (filledBoxAt(10.0f, 10.0f, 10.0f, 10.0f)
                .addOutline(boxAt(10.0f, 10.0f, 10.0f, 10.0f))
                .addOutline(boxAt(200.0f, 10.0f, 10.0f, 10.0f))
                .addOutline(boxAt(90.0f, 10.0f, 20.0f, 10.0f))))
        .clip(avg::Obb(avg::Vector2f(100.0f, 100.0f), toWorld));

    auto const& shapes = d.getOutlines().getShapes();
    ASSERT_EQ(2u, shapes.size());

    auto toLocal = toWorld.inverse();
    auto kept = filledBounds(avg::Shape(shapes[0]).transform(toLocal));
    auto cut = filledBounds(avg::Shape(shapes[1]).transform(toLocal));

    EXPECT_NEAR(10.0f, kept.getLeft(), 1.0f);
    EXPECT_NEAR(20.0f, kept.getRight(), 1.0f);
    EXPECT_NEAR(90.0f, cut.getLeft(), 1.0f);
    EXPECT_NEAR(100.0f, cut.getRight(), 1.0f);
    EXPECT_NEAR(10.0f, cut.getBottom(), 1.0f);
    EXPECT_NEAR(20.0f, cut.getTop(), 1.0f);
}

TEST(DrawingOutlines, concatenationAndOverKeepThemOnTheOuterDrawing)
{
    auto a = filledBoxAt(0.0f, 0.0f, 10.0f, 10.0f)
        .addOutline(boxAt(0.0f, 0.0f, 10.0f, 10.0f));
    auto b = filledBoxAt(20.0f, 0.0f, 10.0f, 10.0f)
        .addOutline(boxAt(20.0f, 0.0f, 10.0f, 10.0f));

    auto copied = avg::Drawing(a) + b;
    auto moved = avg::Drawing(a) + avg::Drawing(b);
    auto over = avg::Drawing::over(a, b);

    for (auto const* d : { &copied, &moved, &over })
    {
        EXPECT_EQ((std::vector<avg::Rect>{
                    rectAt(0.0f, 0.0f, 10.0f, 10.0f),
                    rectAt(20.0f, 0.0f, 10.0f, 10.0f)
                    }), boundsOf(d->getOutlines()));
    }

    ASSERT_EQ(2u, over.getElements().size());
    EXPECT_TRUE(std::holds_alternative<avg::Drawing::BackdropScope>(
                over.getElements()[1]));
}

TEST(DrawingOutlines, liftSubDrawingMovesThemOntoTheHolder)
{
    auto inner = filledBoxAt(0.0f, 0.0f, 10.0f, 10.0f)
        .addOutline(boxAt(0.0f, 0.0f, 10.0f, 10.0f));

    avg::Drawing outer(pmr::new_delete_resource());
    auto transform = avg::translate(50.0f, 0.0f);

    auto subDrawing = outer.liftSubDrawing(std::move(inner), transform);

    EXPECT_EQ(1u, subDrawing->elements.size());
    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(50.0f, 0.0f, 10.0f, 10.0f) }),
            boundsOf(outer.getOutlines()));
}

TEST(DrawingOutlines, effectElementsLiftTheirInputsOutlines)
{
    auto content = []()
    {
        return filledBoxAt(10.0f, 10.0f, 20.0f, 20.0f)
            .addOutline(boxAt(10.0f, 10.0f, 20.0f, 20.0f));
    };

    auto faded = avg::groupOpacity(pmr::new_delete_resource(), content(),
            0.5f).translate(avg::Vector2f(5.0f, 0.0f));
    auto blurred = avg::contentBlur(pmr::new_delete_resource(), content(),
            4.0f).translate(avg::Vector2f(5.0f, 0.0f));

    for (auto const* d : { &faded, &blurred })
    {
        ASSERT_EQ(1u, d->getElements().size());
        EXPECT_TRUE(std::holds_alternative<avg::Drawing::EffectElement>(
                    d->getElements()[0]));
        EXPECT_EQ((std::vector<avg::Rect>{
                    rectAt(15.0f, 10.0f, 20.0f, 20.0f) }),
                boundsOf(d->getOutlines()));
    }
}

TEST(OutlineTree, aCastersOutlineIsWhereItIsDrawn)
{
    auto tree = container(placed(100.0f, 0.0f, 400.0f, 400.0f), {
            caster(placed(10.0f, 20.0f, 30.0f, 40.0f))
            });

    auto drawing = drawNode(*tree).first;

    EXPECT_EQ(rectAt(110.0f, 20.0f, 30.0f, 40.0f), drawing.getControlBb());
    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(110.0f, 20.0f, 30.0f, 40.0f) }),
            boundsOf(drawing.getOutlines()));
}

TEST(OutlineTree, containersCollectTheOutlinesOfTheirChildren)
{
    for (auto stacking : { avg::ContainerNode::Stacking::flat,
            avg::ContainerNode::Stacking::stacked })
    {
        auto tree = container(placed(100.0f, 0.0f, 400.0f, 400.0f), {
                caster(placed(0.0f, 0.0f, 10.0f, 10.0f)),
                rect(placed(20.0f, 0.0f, 10.0f, 10.0f)),
                container(avg::Obb(), {
                    caster(placed(40.0f, 0.0f, 10.0f, 10.0f))
                    }, stacking)
                }, stacking);

        EXPECT_EQ((std::vector<avg::Rect>{
                    rectAt(100.0f, 0.0f, 10.0f, 10.0f),
                    rectAt(140.0f, 0.0f, 10.0f, 10.0f)
                    }), boundsOf(outlinesOf(*tree)));
    }
}

TEST(OutlineTree, idTransitionAndSurfaceNodesPassThemThrough)
{
    auto id = std::make_shared<avg::IdNode>(avg::UniqueId(),
            placed(10.0f, 0.0f, 400.0f, 400.0f),
            caster(placed(0.0f, 0.0f, 10.0f, 10.0f)));

    auto transition = std::make_shared<avg::TransitionNode>(
            placed(0.0f, 10.0f, 400.0f, 400.0f), true, id, nullptr);

    auto surface = std::make_shared<avg::SurfaceNode>(
            avg::Obb(avg::Vector2f(400.0f, 400.0f)),
            std::nullopt,
            avg::makeShapeFunction(&box),
            std::make_optional(avg::Brush(white)),
            std::nullopt,
            transition
            );

    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(0.0f, 0.0f, 400.0f, 400.0f),
                rectAt(10.0f, 10.0f, 10.0f, 10.0f) }),
            boundsOf(outlinesOf(*surface)));
}

TEST(OutlineTree, aClipNodeCutsAndDropsThem)
{
    auto clip = std::make_shared<avg::ClipNode>(
            placed(0.0f, 0.0f, 100.0f, 100.0f),
            container(avg::Obb(), {
                caster(placed(10.0f, 10.0f, 10.0f, 10.0f)),
                caster(placed(200.0f, 10.0f, 10.0f, 10.0f)),
                caster(placed(90.0f, 10.0f, 20.0f, 10.0f))
                }));

    auto outlines = outlinesOf(*clip);

    ASSERT_EQ(2u, outlines.getShapes().size());
    EXPECT_EQ(rectAt(10.0f, 10.0f, 10.0f, 10.0f),
            outlines.getShapes()[0].getControlBb());
    EXPECT_NEAR(100.0f, filledBounds(outlines.getShapes()[1]).getRight(),
            0.5f);
}

TEST(OutlineTree, theRootDropsThem)
{
    avg::RenderTree tree(container(avg::Obb(), {
                caster(placed(0.0f, 0.0f, 10.0f, 10.0f))
                }));

    avg::DrawContext context(pmr::new_delete_resource());
    avg::Obb const obb(avg::Vector2f(400.0f, 400.0f));

    EXPECT_EQ(1u, tree.getRoot()->draw(context, obb, zero).first
            .getOutlines().getShapes().size());

    auto drawing = tree.draw(context, obb, zero).first;

    EXPECT_EQ(1u, drawing.getElements().size());
    EXPECT_TRUE(drawing.getOutlines().isEmpty());
}

TEST(OutlineNode, replaceAddAndNone)
{
    auto child = [] { return caster(placed(0.0f, 0.0f, 10.0f, 10.0f)); };
    auto area = placed(0.0f, 0.0f, 20.0f, 20.0f);

    auto replaced = outlinesOf(*outlined(area, child()));
    auto added = outlinesOf(*outlined(area, child(),
                avg::OutlineNode::Mode::add));
    auto [none, cont] = drawNode(*outlined(area, child(),
                avg::OutlineNode::Mode::replace, std::nullopt));

    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(0.0f, 0.0f, 20.0f, 20.0f) }),
            boundsOf(replaced));
    EXPECT_EQ((std::vector<avg::Rect>{
                rectAt(0.0f, 0.0f, 10.0f, 10.0f),
                rectAt(0.0f, 0.0f, 20.0f, 20.0f)
                }), boundsOf(added));
    EXPECT_TRUE(none.getOutlines().isEmpty());
    EXPECT_EQ(1u, none.getElements().size());
    EXPECT_FALSE(cont);
}

TEST(OutlineNode, theOutlineIsWhereTheChildIsDrawn)
{
    auto area = placed(10.0f, 20.0f, 30.0f, 40.0f);

    auto tree = avg::RenderTree(outlined(area, rect(area)))
        .transform(avg::translate(100.0f, 0.0f));

    auto drawing = drawNode(*tree.getRoot()).first;

    EXPECT_EQ(rectAt(110.0f, 20.0f, 30.0f, 40.0f), drawing.getControlBb());
    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(110.0f, 20.0f, 30.0f, 40.0f) }),
            boundsOf(drawing.getOutlines()));
}

TEST(OutlineNode, itsOwnAnimationResizesButDoesNotMove)
{
    std::chrono::milliseconds const duration(1000);
    avg::AnimationOptions const options{ duration, avg::curve::linear };

    auto makeTree = [&options](float x, float width)
    {
        auto area = placed(0.0f, 0.0f, width, 10.0f);

        return avg::RenderTree(outlined(area, rect(area, options),
                    avg::OutlineNode::Mode::replace,
                    avg::makeShapeFunction(&box), options))
            .transform(avg::translate(x, 0.0f));
    };

    auto [updated, nextUpdate] = makeTree(0.0f, 10.0f)
        .update(makeTree(100.0f, 30.0f), std::nullopt, zero);

    auto [halfway, cont] = drawNode(*updated.getRoot(), duration / 2);

    EXPECT_TRUE(cont);
    EXPECT_EQ(rectAt(100.0f, 0.0f, 20.0f, 10.0f), halfway.getControlBb());
    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(100.0f, 0.0f, 20.0f, 10.0f) }),
            boundsOf(halfway.getOutlines()));
}

TEST(OutlineNode, itsOutlineParametersAnimate)
{
    std::chrono::milliseconds const duration(1000);
    avg::AnimationOptions const options{ duration, avg::curve::linear };
    auto area = placed(0.0f, 0.0f, 40.0f, 40.0f);

    auto makeTree = [&](float side)
    {
        return avg::RenderTree(outlined(area, rect(area),
                    avg::OutlineNode::Mode::replace,
                    avg::makeShapeFunction(&square,
                        avg::Animated<float>(side)),
                    options));
    };

    auto [updated, nextUpdate] = makeTree(10.0f)
        .update(makeTree(30.0f), std::nullopt, zero);

    auto [halfway, cont] = drawNode(*updated.getRoot(), duration / 2);

    EXPECT_TRUE(cont);
    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(0.0f, 0.0f, 20.0f, 20.0f) }),
            boundsOf(halfway.getOutlines()));
}

TEST(OutlineNode, anOldNodeOfAnotherKindIsNotPaired)
{
    auto makeTree = [](std::shared_ptr<avg::RenderTreeNode> child)
    {
        return avg::RenderTree(container(avg::Obb(), { std::move(child) }));
    };

    auto area = placed(20.0f, 0.0f, 10.0f, 10.0f);

    auto [updated, nextUpdate] = makeTree(container(avg::Obb(), {
                rect(placed(0.0f, 0.0f, 10.0f, 10.0f))
                }))
        .update(makeTree(outlined(area, rect(area))), std::nullopt, zero);

    auto drawing = drawNode(*updated.getRoot()).first;

    EXPECT_EQ(rectAt(20.0f, 0.0f, 10.0f, 10.0f), drawing.getControlBb());
    EXPECT_EQ((std::vector<avg::Rect>{ rectAt(20.0f, 0.0f, 10.0f, 10.0f) }),
            boundsOf(drawing.getOutlines()));
}

TEST(OutlineNode, aChildWhoseIdChangesIsNotPaired)
{
    auto area = placed(0.0f, 0.0f, 10.0f, 10.0f);
    auto makeTree = [&](float x)
    {
        return avg::RenderTree(outlined(area,
                    std::make_shared<avg::IdNode>(avg::UniqueId(), avg::Obb(),
                        rect(placed(x, 0.0f, 10.0f, 10.0f)))));
    };

    auto [updated, nextUpdate] = makeTree(0.0f)
        .update(makeTree(20.0f), std::nullopt, zero);

    EXPECT_EQ(rectAt(20.0f, 0.0f, 10.0f, 10.0f),
            drawNode(*updated.getRoot()).first.getControlBb());
}
