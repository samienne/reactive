#include <avg/brush.h>
#include <avg/color.h>
#include <avg/drawcontext.h>
#include <avg/drawing.h>
#include <avg/obb.h>
#include <avg/rect.h>
#include <avg/rendertree.h>
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
