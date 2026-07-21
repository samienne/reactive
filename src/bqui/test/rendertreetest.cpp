#include <bqui/modifier/ondraw.h>
#include <bqui/modifier/setsizehint.h>

#include <bqui/widget/hbox.h>
#include <bqui/widget/widget.h>

#include <bqui/buildparams.h>
#include <bqui/simplesizehint.h>
#include <bqui/sizehint.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/input.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <avg/animationoptions.h>
#include <avg/brush.h>
#include <avg/color.h>
#include <avg/curve/curves.h>
#include <avg/drawcontext.h>
#include <avg/drawing.h>
#include <avg/obb.h>
#include <avg/rect.h>
#include <avg/rendertree.h>
#include <avg/vector.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace bqui;
using namespace bqui::widget;

namespace
{

using Milliseconds = std::chrono::milliseconds;
using Children = std::vector<bq::signal::ArraySignal<AnyWidget>>;

/**
 * @brief Where one probe was drawn, and which probe it was.
 *
 * The color is the identity: it is carried by the draw function a render
 * tree node holds, so it says which probe's drawing this is however the tree
 * was spliced. The bounds are in window coordinates, with the origin at the
 * bottom-left corner.
 */
struct DrawnRect
{
    avg::Color color;
    avg::Rect bounds;
};

avg::Color const red(1.0f, 0.0f, 0.0f, 1.0f);
avg::Color const green(0.0f, 1.0f, 0.0f, 1.0f);
avg::Color const blue(0.0f, 0.0f, 1.0f, 1.0f);

SizeHintResult const fixed50 = {{ 50.0f, 50.0f, 50.0f }};
SizeHintResult const fixed100 = {{ 100.0f, 100.0f, 100.0f }};
SizeHintResult const fixed150 = {{ 150.0f, 150.0f, 150.0f }};

avg::Color colorOf(size_t index)
{
    switch (index)
    {
    case 0:
        return red;
    case 1:
        return green;
    default:
        return blue;
    }
}

SizeHint hintOf(size_t index)
{
    switch (index)
    {
    case 0:
        return simpleSizeHint(fixed100, fixed50);
    case 1:
        return simpleSizeHint(fixed50, fixed50);
    default:
        return simpleSizeHint(fixed150, fixed50);
    }
}

avg::Drawing fillSlot(avg::DrawContext const& context, avg::Vector2f size,
        avg::Color const& color)
{
    return context.pathBuilder()
        .start(0.0f, 0.0f)
        .lineTo(size[0], 0.0f)
        .lineTo(size[0], size[1])
        .lineTo(0.0f, size[1])
        .close()
        .fill(avg::Brush(color));
}

/**
 * @brief A widget that fills whatever it is given, in the color @p index
 *        names.
 *
 * Unlike an input area, what this leaves behind is reachable only by drawing:
 * it is a node in the render tree and nowhere else. The color rides on that
 * node, so it says which probe a drawn shape came from however the tree was
 * spliced.
 */
AnyWidget drawProbe(bq::signal::AnySignal<size_t> index)
{
    auto shared = index.share();

    return makeWidget()
        | modifier::setSizeHint(shared.map(&hintOf))
        | modifier::onDraw(&fillSlot, shared.map(&colorOf))
        ;
}

/** @overload */
AnyWidget drawProbe(size_t index)
{
    return drawProbe(bq::signal::constant(index));
}

/**
 * @brief A widget that takes up the same room but draws nothing at all.
 */
AnyWidget blankProbe(size_t index)
{
    return makeWidget()
        | modifier::setSizeHint(hintOf(index))
        ;
}

/**
 * @brief Splices and draws a render tree the way the window loop does.
 *
 * Every realised Instance is spliced onto the tree the previous one produced,
 * which is what turns a change in the widget tree into an animation and what
 * makes the tree pair its nodes up. Drawing is a pure function of the
 * timestamp, so a draw part-way through an animation shows which node is
 * moving where.
 */
class TreeDriver
{
public:
    TreeDriver(avg::Vector2f size, Milliseconds animationDuration) :
        size_(size),
        animationOptions_(avg::AnimationOptions{
                animationDuration, avg::curve::linear })
    {
    }

    void splice(Instance const& instance, Milliseconds time)
    {
        avg::RenderTree next = instance.getRenderTree();

        tree_ = std::move(std::move(tree_).update(
                std::move(next),
                animationOptions_,
                time
                ).first);
    }

    std::vector<DrawnRect> draw(Milliseconds time)
    {
        avg::DrawContext context(pmr::new_delete_resource());

        auto [drawing, animating] = tree_.draw(context, avg::Obb(size_), time);

        animating_ = animating;

        std::vector<DrawnRect> result;

        for (auto const& element : drawing.getElements())
        {
            auto const* shape = std::get_if<avg::Drawing::ShapeElement>(
                    &element);

            if (!shape || !shape->brush)
                continue;

            result.push_back(DrawnRect{
                    shape->brush->getColor(),
                    shape->shape.getControlBb()
                    });
        }

        return result;
    }

    /**
     * @brief Whether the last draw left anything still moving.
     */
    bool animating() const
    {
        return animating_;
    }

private:
    avg::RenderTree tree_;
    avg::Vector2f size_;
    std::optional<avg::AnimationOptions> animationOptions_;
    bool animating_ = false;
};

std::optional<avg::Rect> boundsOf(std::vector<DrawnRect> const& rects,
        avg::Color const& color)
{
    for (auto const& rect : rects)
        if (rect.color == color)
            return rect.bounds;

    return std::nullopt;
}

void expectDrawnAt(std::string const& label,
        std::vector<DrawnRect> const& rects, avg::Color const& color,
        float x, float y, float width, float height)
{
    SCOPED_TRACE(label);

    auto bounds = boundsOf(rects, color);

    ASSERT_TRUE(bounds.has_value()) << "probe was not drawn";

    EXPECT_FLOAT_EQ(x, bounds->getLeft());
    EXPECT_FLOAT_EQ(y, bounds->getBottom());
    EXPECT_FLOAT_EQ(width, bounds->getWidth());
    EXPECT_FLOAT_EQ(height, bounds->getHeight());
}

void expectNotDrawn(std::string const& label,
        std::vector<DrawnRect> const& rects, avg::Color const& color)
{
    SCOPED_TRACE(label);

    EXPECT_FALSE(boundsOf(rects, color).has_value());
}

avg::Vector2f const row(300.0f, 50.0f);

Milliseconds const duration(100);
Milliseconds const start(0);
Milliseconds const halfway(50);

/**
 * @brief A row of probes whose membership the test drives.
 *
 * Each probe is keyed by the index that names it, so a probe keeps its
 * identity across removals and reorderings.
 */
AnyWidget dynamicRow(bq::signal::AnySignal<std::vector<size_t>> indices)
{
    return hbox(bq::signal::forEach(std::move(indices),
            [](size_t index)
            {
                return index;
            },
            [](bq::signal::AnySignal<size_t> index)
            {
                return drawProbe(std::move(index));
            }));
}

bq::signal::FrameInfo nextFrame(uint64_t frameId)
{
    return bq::signal::FrameInfo(frameId, std::chrono::microseconds(0));
}

} // anonymous namespace

TEST(RenderTree, everyChildDrawsInTheSlotTheLayoutGaveIt)
{
    // Where a child ends up on screen is decided entirely inside the render
    // tree: the layout hands each child a transform, every enclosing node
    // contributes its own on the way down, and only the drawing says what the
    // sum of them was. A node that repeats a transform its child already
    // carries draws that child at twice the offset it was laid out at.
    Children children;
    children.push_back(drawProbe(0));
    children.push_back(drawProbe(1));
    children.push_back(drawProbe(2));

    auto context = bq::signal::makeSignalContext(
            hbox(std::move(children))(BuildParams())(
                bq::signal::constant(row)).getInstance());

    TreeDriver driver(row, duration);
    driver.splice(context.evaluate<0>().get<0>(), start);

    auto rects = driver.draw(start);

    ASSERT_EQ(3u, rects.size());

    expectDrawnAt("first", rects, red, 0.0f, 0.0f, 100.0f, 50.0f);
    expectDrawnAt("second", rects, green, 100.0f, 0.0f, 50.0f, 50.0f);
    expectDrawnAt("third", rects, blue, 150.0f, 0.0f, 150.0f, 50.0f);
}

TEST(RenderTree, aChildThatDrawsNothingContributesNothing)
{
    // A layout names each of its children so that the tree can pair them up,
    // and a child that draws nothing has no node to name. Naming one anyway
    // leaves a node that cannot be drawn, which nothing notices until the
    // first frame.
    Children children;
    children.push_back(drawProbe(0));
    children.push_back(blankProbe(1));
    children.push_back(drawProbe(2));

    auto context = bq::signal::makeSignalContext(
            hbox(std::move(children))(BuildParams())(
                bq::signal::constant(row)).getInstance());

    TreeDriver driver(row, duration);
    driver.splice(context.evaluate<0>().get<0>(), start);

    auto rects = driver.draw(start);

    ASSERT_EQ(2u, rects.size());

    expectDrawnAt("first", rects, red, 0.0f, 0.0f, 100.0f, 50.0f);
    expectDrawnAt("third", rects, blue, 150.0f, 0.0f, 150.0f, 50.0f);
}

TEST(RenderTree, removingAMiddleChildLeavesItsNeighboursNodes)
{
    // The tree pairs a child up with the child of the same name, so a removal
    // takes out the node that was removed. Pairing by position instead pairs
    // the departing child with whichever one now sits in its slot, and the
    // last child — which nothing now pairs with — is the one that leaves.
    auto input = bq::signal::makeInput(std::vector<size_t>{ 0, 1, 2 });

    auto context = bq::signal::makeSignalContext(
            dynamicRow(input.signal)(BuildParams())(
                bq::signal::constant(row)).getInstance());

    TreeDriver driver(row, duration);
    driver.splice(context.evaluate<0>().get<0>(), start);

    input.handle.set(std::vector<size_t>{ 0, 2 });
    context.update(nextFrame(1));

    driver.splice(context.evaluate<0>().get<0>(), start);

    auto rects = driver.draw(halfway);

    ASSERT_EQ(2u, rects.size());

    expectDrawnAt("stayed put", rects, red, 0.0f, 0.0f, 100.0f, 50.0f);
    expectNotDrawn("removed", rects, green);

    // Half of the way from the slot it had to the one the removal frees up,
    // at the size it has always had.
    expectDrawnAt("moved up", rects, blue, 125.0f, 0.0f, 150.0f, 50.0f);
    EXPECT_TRUE(driver.animating());

    rects = driver.draw(duration);

    ASSERT_EQ(2u, rects.size());

    expectDrawnAt("settled left", rects, red, 0.0f, 0.0f, 100.0f, 50.0f);
    expectDrawnAt("settled right", rects, blue, 100.0f, 0.0f, 150.0f, 50.0f);
    EXPECT_FALSE(driver.animating());
}

TEST(RenderTree, reorderingMovesNodesRatherThanRebuildingThem)
{
    // A node paired with its own name animates from where it was to where it
    // now belongs. One that was rebuilt instead has nothing to animate from
    // and appears at its destination immediately, so a draw part-way through
    // tells the two apart.
    auto input = bq::signal::makeInput(std::vector<size_t>{ 0, 1, 2 });

    auto context = bq::signal::makeSignalContext(
            dynamicRow(input.signal)(BuildParams())(
                bq::signal::constant(row)).getInstance());

    TreeDriver driver(row, duration);
    driver.splice(context.evaluate<0>().get<0>(), start);

    input.handle.set(std::vector<size_t>{ 2, 0, 1 });
    context.update(nextFrame(1));

    driver.splice(context.evaluate<0>().get<0>(), start);

    auto rects = driver.draw(halfway);

    ASSERT_EQ(3u, rects.size());

    // Each probe keeps its size and is half of the way from its old slot to
    // its new one: 0 -> 150, 100 -> 250, and 150 -> 0.
    expectDrawnAt("first", rects, red, 75.0f, 0.0f, 100.0f, 50.0f);
    expectDrawnAt("second", rects, green, 175.0f, 0.0f, 50.0f, 50.0f);
    expectDrawnAt("third", rects, blue, 75.0f, 0.0f, 150.0f, 50.0f);
    EXPECT_TRUE(driver.animating());

    rects = driver.draw(duration);

    expectDrawnAt("first settled", rects, red, 150.0f, 0.0f, 100.0f, 50.0f);
    expectDrawnAt("second settled", rects, green, 250.0f, 0.0f, 50.0f, 50.0f);
    expectDrawnAt("third settled", rects, blue, 0.0f, 0.0f, 150.0f, 50.0f);
    EXPECT_FALSE(driver.animating());
}
