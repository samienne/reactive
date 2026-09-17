#include <avg/animationoptions.h>
#include <avg/brush.h>
#include <avg/color.h>
#include <avg/curve/curves.h>
#include <avg/drawcontext.h>
#include <avg/obb.h>
#include <avg/rendertree.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <utility>

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

// An id'd subtree wrapping a transition, matching how a leaving widget is
// represented in the render tree.
std::shared_ptr<avg::RenderTreeNode> leaving(avg::UniqueId const& id)
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
}

std::shared_ptr<avg::ContainerNode> containerWith(
        std::shared_ptr<avg::RenderTreeNode> child)
{
    auto container = std::make_shared<avg::ContainerNode>(
            avg::Obb(avg::Vector2f(300.0f, 50.0f)));

    if (child)
        container->addChild(std::move(child));

    return container;
}

avg::Snapshot snapshotOf(avg::RenderTree const& tree)
{
    avg::DrawContext context(pmr::new_delete_resource());

    return tree.snapshot(context, avg::Obb(avg::Vector2f(300.0f, 50.0f)), zero);
}

} // anonymous namespace

// A node that disappears with no animation options has nothing to animate, so
// it must be dropped from the tree immediately. Before the fix it fell through
// to the transitioned child's update, which returns non-null, and the parent
// container retained it forever.
TEST(TransitionNode, aDisappearWithoutAnimationIsDroppedImmediately)
{
    auto id = avg::UniqueId();

    auto [tree, nextUpdate] = avg::RenderTree(containerWith(leaving(id))).update(
            avg::RenderTree(containerWith(nullptr)),
            std::nullopt,
            zero
            );

    auto snapshot = snapshotOf(tree);

    ASSERT_TRUE(snapshot.root.has_value());
    EXPECT_TRUE(snapshot.root->children.empty());
    EXPECT_FALSE(nextUpdate.has_value());
}

// With animation options the leaving node is retained for the duration of the
// transition and only pruned once the transition has run its course.
TEST(TransitionNode, aDisappearWithAnimationIsRetainedThenPruned)
{
    auto id = avg::UniqueId();

    avg::AnimationOptions options { std::chrono::milliseconds(100),
        avg::curve::linear };

    // Kick off the leave transition at t=0: the node is kept, marked leaving.
    auto [tree0, nextUpdate0] = avg::RenderTree(containerWith(leaving(id)))
        .update(
                avg::RenderTree(containerWith(nullptr)),
                std::make_optional(options),
                zero
                );

    {
        auto snapshot = snapshotOf(tree0);
        ASSERT_TRUE(snapshot.root.has_value());
        ASSERT_EQ(1u, snapshot.root->children.size());
        EXPECT_TRUE(snapshot.root->children[0].leaving);
    }
    ASSERT_TRUE(nextUpdate0.has_value());
    EXPECT_EQ(std::chrono::milliseconds(100), *nextUpdate0);

    // Mid-transition it is still present and animating.
    auto [tree1, nextUpdate1] = std::move(tree0).update(
            avg::RenderTree(containerWith(nullptr)),
            std::make_optional(options),
            std::chrono::milliseconds(50)
            );

    {
        auto snapshot = snapshotOf(tree1);
        ASSERT_TRUE(snapshot.root.has_value());
        ASSERT_EQ(1u, snapshot.root->children.size());
        EXPECT_TRUE(snapshot.root->children[0].leaving);
    }

    // Once the transition has elapsed the node is pruned.
    auto [tree2, nextUpdate2] = std::move(tree1).update(
            avg::RenderTree(containerWith(nullptr)),
            std::make_optional(options),
            std::chrono::milliseconds(100)
            );

    auto snapshot = snapshotOf(tree2);
    ASSERT_TRUE(snapshot.root.has_value());
    EXPECT_TRUE(snapshot.root->children.empty());
}
