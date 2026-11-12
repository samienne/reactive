#include <avg/brush.h>
#include <avg/color.h>
#include <avg/drawcontext.h>
#include <avg/drawing.h>
#include <avg/obb.h>
#include <avg/pathbuilder.h>
#include <avg/pen.h>
#include <avg/rect.h>
#include <avg/rendertree.h>
#include <avg/shape.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace
{

std::chrono::milliseconds const zero(0);

avg::Obb placed(float x, float y, float width, float height)
{
    return avg::Obb(avg::Vector2f(width, height), avg::translate(x, y));
}

std::shared_ptr<avg::RectNode> rect(avg::Obb const& obb)
{
    return std::make_shared<avg::RectNode>(
            obb,
            std::nullopt,
            0.0f,
            std::make_optional(avg::Brush(avg::Color(1.0f, 1.0f, 1.0f, 1.0f))),
            std::nullopt
            );
}

/**
 * @brief The bounding rectangle of every shape a drawing holds, in order.
 */
std::vector<avg::Rect> shapeBounds(avg::Drawing const& drawing)
{
    std::vector<avg::Rect> result;

    for (auto const& element : drawing.getElements())
    {
        auto const* shape = std::get_if<avg::Drawing::ShapeElement>(&element);
        if (shape)
            result.push_back(shape->shape.getControlBb());
    }

    return result;
}

void expectRect(avg::Rect const& r, float x, float y,
        float width, float height)
{
    EXPECT_FLOAT_EQ(x, r.getLeft());
    EXPECT_FLOAT_EQ(y, r.getBottom());
    EXPECT_FLOAT_EQ(width, r.getWidth());
    EXPECT_FLOAT_EQ(height, r.getHeight());
}

} // anonymous namespace

TEST(Draw, contextOffAMemoryResourceAllocatesFromIt)
{
    avg::DrawContext context(pmr::new_delete_resource());

    EXPECT_EQ(pmr::new_delete_resource(), context.getResource());
    EXPECT_EQ(pmr::new_delete_resource(), context.drawing().getResource());
    EXPECT_EQ(pmr::new_delete_resource(),
            context.pathBuilder().build().getResource());
}

TEST(Draw, treeDrawsWithoutARenderingBackEnd)
{
    avg::DrawContext context(pmr::new_delete_resource());

    auto container = std::make_shared<avg::ContainerNode>(
            avg::Obb(avg::Vector2f(300.0f, 50.0f)));

    container->addChild(rect(placed(0.0f, 0.0f, 100.0f, 50.0f)));
    container->addChild(rect(placed(100.0f, 0.0f, 50.0f, 50.0f)));

    auto [drawing, cont] = avg::RenderTree(std::move(container)).draw(
            context,
            avg::Obb(avg::Vector2f(300.0f, 50.0f)),
            zero
            );

    EXPECT_FALSE(cont);

    auto bounds = shapeBounds(drawing);

    ASSERT_EQ(2u, bounds.size());
    expectRect(bounds[0], 0.0f, 0.0f, 100.0f, 50.0f);
    expectRect(bounds[1], 100.0f, 0.0f, 50.0f, 50.0f);
}

TEST(Draw, everyEnclosingObbCompoundsOntoAChild)
{
    avg::DrawContext context(pmr::new_delete_resource());

    auto container = std::make_shared<avg::ContainerNode>(
            placed(10.0f, 20.0f, 300.0f, 50.0f));

    container->addChild(rect(placed(5.0f, 5.0f, 100.0f, 50.0f)));

    auto bounds = shapeBounds(avg::RenderTree(std::move(container)).draw(
            context,
            placed(1.0f, 2.0f, 300.0f, 50.0f),
            zero
            ).first);

    ASSERT_EQ(1u, bounds.size());
    expectRect(bounds[0], 16.0f, 27.0f, 100.0f, 50.0f);
}

namespace
{

avg::Drawing::Element square(float x, float size)
{
    return avg::Drawing::ShapeElement{
        avg::Shape(avg::PathBuilder(pmr::new_delete_resource())
                .start(x, 0.0f)
                .lineTo(x + size, 0.0f)
                .lineTo(x + size, size)
                .lineTo(x, size)
                .close()
                .build()),
        avg::Brush(avg::Color(1.0f, 1.0f, 1.0f, 1.0f)),
        std::nullopt
    };
}

avg::Drawing::Element stroked(avg::Drawing::Element element, float width)
{
    std::get<avg::Drawing::ShapeElement>(element).pen = avg::Pen(
            avg::Brush(avg::Color(1.0f, 1.0f, 1.0f, 1.0f)), width);

    return element;
}

avg::Drawing drawing(std::vector<avg::Drawing::Element> elements)
{
    avg::Drawing result(pmr::new_delete_resource());
    for (auto&& e : elements)
        result += std::move(e);

    return result;
}

/**
 * @brief The left edge of every shape in a list, in order; a scope or a clip
 * contributes -1 and its contents are not entered.
 */
std::vector<float> lefts(pmr::vector<avg::Drawing::Element> const& elements)
{
    std::vector<float> result;

    for (auto const& element : elements)
    {
        auto const* shape = std::get_if<avg::Drawing::ShapeElement>(&element);
        result.push_back(shape ? shape->shape.getControlBb().getLeft() : -1.0f);
    }

    return result;
}

avg::Drawing::BackdropScope const& scopeAt(
        pmr::vector<avg::Drawing::Element> const& elements, size_t i)
{
    return std::get<avg::Drawing::BackdropScope>(elements.at(i));
}

avg::Drawing::ClipElement const& clipAt(
        pmr::vector<avg::Drawing::Element> const& elements, size_t i)
{
    return std::get<avg::Drawing::ClipElement>(elements.at(i));
}

} // anonymous namespace

TEST(Drawing, scopeWrapsTheMainListAndLeavesShadowsPending)
{
    auto d = drawing({ square(0.0f, 10.0f), square(20.0f, 10.0f) })
        .addShadow(square(40.0f, 10.0f))
        .scope();

    ASSERT_EQ(1u, d.getElements().size());
    EXPECT_EQ((std::vector<float>{ 0.0f, 20.0f }),
            lefts(scopeAt(d.getElements(), 0).subDrawing->elements));
    EXPECT_EQ((std::vector<float>{ 40.0f }), lefts(d.getPendingShadows()));

    auto empty = avg::Drawing(pmr::new_delete_resource()).scope();
    EXPECT_TRUE(empty.getElements().empty());
}

TEST(Drawing, concatenationKeepsScopesNestedAndAppendsShadows)
{
    auto a = drawing({ square(0.0f, 10.0f) })
        .addShadow(square(100.0f, 10.0f))
        .scope();
    auto b = drawing({ square(20.0f, 10.0f) }).scope()
        .addShadow(square(200.0f, 10.0f))
        .scope();

    a += b;

    ASSERT_EQ(2u, a.getElements().size());
    EXPECT_EQ((std::vector<float>{ 0.0f }),
            lefts(scopeAt(a.getElements(), 0).subDrawing->elements));

    auto const& outer = scopeAt(a.getElements(), 1).subDrawing->elements;
    ASSERT_EQ(1u, outer.size());
    EXPECT_EQ((std::vector<float>{ 20.0f }),
            lefts(scopeAt(outer, 0).subDrawing->elements));

    EXPECT_EQ((std::vector<float>{ 100.0f, 200.0f }),
            lefts(a.getPendingShadows()));
}

TEST(Drawing, clipWrapsEachListAndNeverInventsAShadow)
{
    auto d = drawing({ square(0.0f, 10.0f) }).scope()
        .addShadow(square(0.0f, 30.0f))
        .clip(avg::Rect(avg::Vector2f(0.0f, 0.0f), avg::Vector2f(20.0f, 20.0f)));

    ASSERT_EQ(1u, d.getElements().size());
    auto const& clipped = clipAt(d.getElements(), 0).subDrawing->elements;
    ASSERT_EQ(1u, clipped.size());
    EXPECT_EQ((std::vector<float>{ 0.0f }),
            lefts(scopeAt(clipped, 0).subDrawing->elements));

    ASSERT_EQ(1u, d.getPendingShadows().size());
    EXPECT_EQ((std::vector<float>{ 0.0f }),
            lefts(clipAt(d.getPendingShadows(), 0).subDrawing->elements));
    expectRect(d.getControlBb(), 0.0f, 0.0f, 20.0f, 20.0f);

    auto shadowless = drawing({ square(0.0f, 30.0f) })
        .clip(avg::Rect(avg::Vector2f(0.0f, 0.0f), avg::Vector2f(20.0f, 20.0f)));
    EXPECT_EQ(1u, shadowless.getElements().size());
    EXPECT_TRUE(shadowless.getPendingShadows().empty());
}

TEST(Drawing, clipLeavesOutAShadowListItEmpties)
{
    auto d = drawing({ square(0.0f, 10.0f) })
        .addShadow(square(40.0f, 10.0f))
        .clip(avg::Rect(avg::Vector2f(0.0f, 0.0f), avg::Vector2f(5.0f, 5.0f)));

    EXPECT_EQ(1u, d.getElements().size());
    EXPECT_TRUE(d.getPendingShadows().empty());
}

TEST(Drawing, scopeBoundsIncludeStrokes)
{
    auto d = drawing({ stroked(square(0.0f, 10.0f), 4.0f) }).scope();

    expectRect(scopeAt(d.getElements(), 0).controlBb,
            -2.0f, -2.0f, 14.0f, 14.0f);
}

TEST(Drawing, filterByRectKeepsAnElementWhoseStrokeReachesTheRect)
{
    auto d = drawing({
            stroked(square(0.0f, 10.0f), 4.0f),
            square(0.0f, 10.0f)
            })
        .filterByRect(avg::Rect(
                    avg::Vector2f(11.0f, 0.0f), avg::Vector2f(10.0f, 10.0f)));

    ASSERT_EQ(1u, d.getElements().size());
    EXPECT_TRUE(std::get<avg::Drawing::ShapeElement>(d.getElements().at(0))
            .pen.has_value());
}

TEST(Drawing, transformMovesScopesAndShadowsAlike)
{
    auto d = drawing({ square(0.0f, 10.0f) }).scope()
        .addShadow(square(0.0f, 10.0f))
        .translate(avg::Vector2f(5.0f, 7.0f));

    auto const& scope = scopeAt(d.getElements(), 0);
    EXPECT_FLOAT_EQ(5.0f, scope.transform.getTranslation().x());
    EXPECT_FLOAT_EQ(7.0f, scope.transform.getTranslation().y());
    expectRect(scope.controlBb, 0.0f, 0.0f, 10.0f, 10.0f);

    EXPECT_EQ((std::vector<float>{ 5.0f }), lefts(d.getPendingShadows()));
    expectRect(d.getControlBb(), 5.0f, 7.0f, 10.0f, 10.0f);
}

TEST(Drawing, flushPutsShadowsAheadOfContentAndEmptiesThem)
{
    auto d = drawing({ square(0.0f, 10.0f) })
        .addShadow(square(100.0f, 10.0f))
        .addShadow(square(200.0f, 10.0f))
        .flush();

    EXPECT_EQ((std::vector<float>{ 100.0f, 200.0f, 0.0f }),
            lefts(d.getElements()));
    EXPECT_TRUE(d.getPendingShadows().empty());

    auto unchanged = drawing({ square(0.0f, 10.0f) }).flush();
    EXPECT_EQ((std::vector<float>{ 0.0f }), lefts(unchanged.getElements()));
}

TEST(Drawing, unflushedShadowsStayOutOfTheMainList)
{
    auto d = drawing({ square(0.0f, 10.0f) }).addShadow(square(100.0f, 10.0f));

    EXPECT_EQ((std::vector<float>{ 0.0f }), lefts(d.getElements()));
    EXPECT_EQ((std::vector<float>{ 100.0f }), lefts(d.getPendingShadows()));
    expectRect(d.getControlBb(), 0.0f, 0.0f, 110.0f, 10.0f);
}

TEST(Drawing, overPaintsBelowThenShadowsThenScopedAbove)
{
    auto below = drawing({ square(0.0f, 10.0f) }).addShadow(square(50.0f, 10.0f));
    auto above = drawing({ square(10.0f, 10.0f), square(20.0f, 10.0f) })
        .addShadow(square(60.0f, 10.0f));

    auto d = avg::Drawing::over(std::move(below), std::move(above));

    EXPECT_EQ((std::vector<float>{ 0.0f, 60.0f, -1.0f }), lefts(d.getElements()));
    EXPECT_EQ((std::vector<float>{ 10.0f, 20.0f }),
            lefts(scopeAt(d.getElements(), 2).subDrawing->elements));
    EXPECT_EQ((std::vector<float>{ 50.0f }), lefts(d.getPendingShadows()));
    expectRect(d.getControlBb(), 0.0f, 0.0f, 70.0f, 10.0f);

    auto flat = avg::Drawing::over(drawing({ square(0.0f, 10.0f) }),
            avg::Drawing(pmr::new_delete_resource()));
    EXPECT_EQ((std::vector<float>{ 0.0f }), lefts(flat.getElements()));
}
