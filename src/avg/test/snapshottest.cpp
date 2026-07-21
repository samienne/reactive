#include <avg/brush.h>
#include <avg/animationoptions.h>
#include <avg/color.h>
#include <avg/curve/curves.h>
#include <avg/drawcontext.h>
#include <avg/drawing.h>
#include <avg/font.h>
#include <avg/obb.h>
#include <avg/rect.h>
#include <avg/rendertree.h>
#include <avg/textentry.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{

std::chrono::milliseconds const zero(0);

std::string const fontPath = "data/fonts/OpenSans-Regular.ttf";

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

std::shared_ptr<avg::RenderTreeNode> text(avg::Obb const& obb,
        std::string const& content)
{
    avg::Font font(fontPath, 0);

    std::function<avg::Drawing(avg::DrawContext const&, avg::Vector2f)> draw =
        [font, content](avg::DrawContext const& context, avg::Vector2f)
        {
            return context.drawing(avg::TextEntry(
                        font,
                        avg::Transform().scale(10.0f),
                        content,
                        std::make_optional(avg::Brush(
                                avg::Color(1.0f, 1.0f, 1.0f, 1.0f))),
                        std::nullopt
                        ));
        };

    return avg::makeShapeNode(obb, std::nullopt, std::move(draw));
}

avg::Snapshot snapshotOf(avg::RenderTree const& tree, avg::Obb const& obb)
{
    avg::DrawContext context(pmr::new_delete_resource());

    return tree.snapshot(context, obb, zero);
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

void expectObb(avg::Obb const& obb, float x, float y,
        float width, float height)
{
    auto r = obb.getBoundingRect();

    EXPECT_FLOAT_EQ(x, r.getLeft());
    EXPECT_FLOAT_EQ(y, r.getBottom());
    EXPECT_FLOAT_EQ(width, r.getWidth());
    EXPECT_FLOAT_EQ(height, r.getHeight());
}

} // anonymous namespace

TEST(Snapshot, describesTypeIdentityAndNesting)
{
    auto id = avg::UniqueId();

    auto container = std::make_shared<avg::ContainerNode>(
            avg::Obb(avg::Vector2f(300.0f, 50.0f)));

    container->addChild(std::make_shared<avg::IdNode>(
                id,
                placed(10.0f, 0.0f, 100.0f, 50.0f),
                rect(placed(0.0f, 0.0f, 100.0f, 50.0f))
                ));

    container->addChild(std::make_shared<avg::ClipNode>(
                placed(200.0f, 0.0f, 50.0f, 50.0f),
                rect(placed(0.0f, 0.0f, 80.0f, 50.0f))
                ));

    auto snapshot = snapshotOf(
            avg::RenderTree(std::move(container)),
            avg::Obb(avg::Vector2f(300.0f, 50.0f))
            );

    ASSERT_TRUE(snapshot.root.has_value());

    auto const& root = *snapshot.root;
    EXPECT_EQ("ContainerNode", root.type);
    EXPECT_FALSE(root.id.has_value());
    ASSERT_EQ(2u, root.children.size());

    auto const& idNode = root.children[0];
    EXPECT_EQ("IdNode", idNode.type);
    ASSERT_TRUE(idNode.id.has_value());
    EXPECT_EQ(id, *idNode.id);
    expectObb(idNode.obb, 10.0f, 0.0f, 100.0f, 50.0f);
    ASSERT_EQ(1u, idNode.children.size());
    EXPECT_EQ("RectNode", idNode.children[0].type);

    auto const& clipNode = root.children[1];
    EXPECT_EQ("ClipNode", clipNode.type);
    expectObb(clipNode.obb, 200.0f, 0.0f, 50.0f, 50.0f);
    ASSERT_EQ(1u, clipNode.children.size());
    EXPECT_EQ("RectNode", clipNode.children[0].type);
}

TEST(Snapshot, geometryIsResolvedToTheSnapshotObb)
{
    auto outer = std::make_shared<avg::ContainerNode>(
            placed(10.0f, 20.0f, 300.0f, 50.0f));

    auto inner = std::make_shared<avg::ContainerNode>(
            placed(1.0f, 2.0f, 300.0f, 50.0f));

    inner->addChild(rect(placed(5.0f, 5.0f, 100.0f, 50.0f)));
    outer->addChild(std::move(inner));

    auto snapshot = snapshotOf(
            avg::RenderTree(std::move(outer)),
            placed(100.0f, 200.0f, 300.0f, 50.0f)
            );

    ASSERT_TRUE(snapshot.root.has_value());

    auto const& root = *snapshot.root;
    expectObb(root.obb, 110.0f, 220.0f, 300.0f, 50.0f);

    ASSERT_EQ(1u, root.children.size());
    expectObb(root.children[0].obb, 111.0f, 222.0f, 300.0f, 50.0f);

    ASSERT_EQ(1u, root.children[0].children.size());
    expectObb(root.children[0].children[0].obb,
            116.0f, 227.0f, 100.0f, 50.0f);
}

TEST(Snapshot, reportsTheTextALeafDraws)
{
    auto container = std::make_shared<avg::ContainerNode>(
            placed(10.0f, 20.0f, 300.0f, 50.0f));

    container->addChild(text(placed(5.0f, 5.0f, 100.0f, 50.0f), "Ok"));

    auto snapshot = snapshotOf(
            avg::RenderTree(std::move(container)),
            avg::Obb(avg::Vector2f(300.0f, 50.0f))
            );

    ASSERT_TRUE(snapshot.root.has_value());
    ASSERT_EQ(1u, snapshot.root->children.size());

    auto const& leaf = snapshot.root->children[0];
    EXPECT_EQ("ShapeNode", leaf.type);
    ASSERT_EQ(1u, leaf.text.size());
    EXPECT_EQ("Ok", leaf.text[0].text);

    auto bounds = leaf.text[0].obb.getBoundingRect();
    EXPECT_FALSE(bounds.isEmpty());

    // The text is placed by every enclosing obb, not just its own.
    EXPECT_GE(bounds.getLeft(), 15.0f);
    EXPECT_GE(bounds.getBottom(), 25.0f);
}

TEST(Snapshot, reportsASubtreeOnItsWayOut)
{
    auto leaving = [](avg::UniqueId const& id)
    {
        return std::make_shared<avg::IdNode>(
                id,
                placed(0.0f, 0.0f, 100.0f, 50.0f),
                std::make_shared<avg::TransitionNode>(
                    placed(0.0f, 0.0f, 100.0f, 50.0f),
                    true,
                    rect(placed(0.0f, 0.0f, 100.0f, 50.0f)),
                    rect(placed(0.0f, 0.0f, 100.0f, 50.0f))
                    )
                );
    };

    auto oldContainer = std::make_shared<avg::ContainerNode>(
            avg::Obb(avg::Vector2f(300.0f, 50.0f)));

    oldContainer->addChild(leaving(avg::UniqueId()));

    auto newContainer = std::make_shared<avg::ContainerNode>(
            avg::Obb(avg::Vector2f(300.0f, 50.0f)));

    avg::AnimationOptions options { std::chrono::milliseconds(100),
        avg::curve::linear };

    auto [tree, nextUpdate] = avg::RenderTree(std::move(oldContainer)).update(
            avg::RenderTree(std::move(newContainer)),
            std::make_optional(options),
            zero
            );

    auto snapshot = snapshotOf(tree, avg::Obb(avg::Vector2f(300.0f, 50.0f)));

    ASSERT_TRUE(snapshot.root.has_value());
    ASSERT_EQ(1u, snapshot.root->children.size());

    auto const& idNode = snapshot.root->children[0];
    EXPECT_EQ("IdNode", idNode.type);
    EXPECT_TRUE(idNode.leaving);

    ASSERT_EQ(1u, idNode.children.size());
    EXPECT_EQ("TransitionNode", idNode.children[0].type);
    EXPECT_TRUE(idNode.children[0].leaving);
}

TEST(Snapshot, anEmptyTreeHasNoRoot)
{
    auto snapshot = snapshotOf(avg::RenderTree(),
            avg::Obb(avg::Vector2f(300.0f, 50.0f)));

    EXPECT_FALSE(snapshot.root.has_value());
    EXPECT_NE(std::string::npos, avg::toJson(snapshot).find("\"root\":null"));
}

TEST(Snapshot, nullSubtreesAreDescribedWithoutChildren)
{
    auto container = std::make_shared<avg::ContainerNode>(
            avg::Obb(avg::Vector2f(300.0f, 50.0f)));

    container->addChild(std::make_shared<avg::IdNode>(
                avg::UniqueId(),
                placed(0.0f, 0.0f, 100.0f, 50.0f),
                nullptr
                ));

    container->addChild(std::make_shared<avg::ClipNode>(
                placed(100.0f, 0.0f, 100.0f, 50.0f),
                nullptr
                ));

    container->addChild(std::make_shared<avg::TransitionNode>(
                placed(200.0f, 0.0f, 100.0f, 50.0f),
                true,
                nullptr,
                nullptr
                ));

    auto snapshot = snapshotOf(
            avg::RenderTree(std::move(container)),
            avg::Obb(avg::Vector2f(300.0f, 50.0f))
            );

    ASSERT_TRUE(snapshot.root.has_value());
    ASSERT_EQ(3u, snapshot.root->children.size());

    for (auto const& child : snapshot.root->children)
        EXPECT_TRUE(child.children.empty());
}

TEST(Snapshot, leavesTheTreeUnchanged)
{
    avg::DrawContext context(pmr::new_delete_resource());

    auto container = std::make_shared<avg::ContainerNode>(
            placed(10.0f, 20.0f, 300.0f, 50.0f));

    container->addChild(rect(placed(5.0f, 5.0f, 100.0f, 50.0f)));

    auto tree = avg::RenderTree(std::move(container));
    auto const* root = tree.getRoot().get();

    auto viewport = avg::Obb(avg::Vector2f(300.0f, 50.0f));

    auto before = shapeBounds(tree.draw(context, viewport, zero).first);

    tree.snapshot(context, viewport, zero);

    auto after = shapeBounds(tree.draw(context, viewport, zero).first);

    EXPECT_EQ(root, tree.getRoot().get());

    ASSERT_EQ(before.size(), after.size());
    for (size_t i = 0; i < before.size(); ++i)
    {
        EXPECT_FLOAT_EQ(before[i].getLeft(), after[i].getLeft());
        EXPECT_FLOAT_EQ(before[i].getBottom(), after[i].getBottom());
        EXPECT_FLOAT_EQ(before[i].getWidth(), after[i].getWidth());
        EXPECT_FLOAT_EQ(before[i].getHeight(), after[i].getHeight());
    }
}

TEST(Snapshot, jsonCarriesTheSchemaVersionAndEscapesText)
{
    auto container = std::make_shared<avg::ContainerNode>(
            avg::Obb(avg::Vector2f(300.0f, 50.0f)));

    auto id = avg::UniqueId();

    container->addChild(std::make_shared<avg::IdNode>(
                id,
                placed(0.0f, 0.0f, 100.0f, 50.0f),
                text(placed(0.0f, 0.0f, 100.0f, 50.0f), "a\"b\nc")
                ));

    auto json = avg::toJson(snapshotOf(
                avg::RenderTree(std::move(container)),
                avg::Obb(avg::Vector2f(300.0f, 50.0f))
                ));

    EXPECT_NE(std::string::npos, json.find("\"version\":1"));
    EXPECT_NE(std::string::npos, json.find("\"type\":\"ContainerNode\""));
    EXPECT_NE(std::string::npos,
            json.find("\"id\":" + std::to_string(id.getValue())));
    EXPECT_NE(std::string::npos, json.find("\"text\":\"a\\\"b\\nc\""));
    EXPECT_NE(std::string::npos, json.find("\"angle\":"));
}
