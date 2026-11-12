#include <avg/animationoptions.h>
#include <avg/brush.h>
#include <avg/builtineffects.h>
#include <avg/color.h>
#include <avg/curve/curves.h>
#include <avg/drawcontext.h>
#include <avg/drawing.h>
#include <avg/effect.h>
#include <avg/obb.h>
#include <avg/rect.h>
#include <avg/rendertree.h>
#include <avg/shape.h>
#include <avg/shapefunction.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{

std::chrono::milliseconds const zero(0);

avg::Color const white(1.0f, 1.0f, 1.0f, 1.0f);
avg::Color const black(0.0f, 0.0f, 0.0f, 1.0f);

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
            std::make_optional(avg::Brush(white)),
            std::nullopt
            );
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

avg::SurfaceShadow shadowAt(float dx, float dy, float spread = 0.0f)
{
    return avg::SurfaceShadow{
        avg::Vector2f(dx, dy), spread, avg::Brush(black)
    };
}

avg::SurfaceEffect glass(float radius)
{
    avg::FrostedGlassOptions options;
    options.radius = radius;

    return avg::SurfaceFrostedGlass{ options };
}

std::shared_ptr<avg::SurfaceNode> surface(avg::Obb const& obb,
        std::optional<avg::SurfaceShadow> shadow,
        std::shared_ptr<avg::RenderTreeNode> child = nullptr,
        std::optional<avg::SurfaceEffect> effect = std::nullopt,
        std::optional<avg::AnimationOptions> options = std::nullopt)
{
    return std::make_shared<avg::SurfaceNode>(
            obb,
            std::move(options),
            avg::makeShapeFunction(&box),
            std::make_optional(avg::Brush(white)),
            std::move(shadow),
            std::move(effect),
            std::move(child)
            );
}

std::shared_ptr<avg::ContainerNode> container(avg::Obb const& obb,
        avg::ContainerNode::Stacking stacking,
        std::vector<std::shared_ptr<avg::RenderTreeNode>> children)
{
    auto result = std::make_shared<avg::ContainerNode>(obb, stacking);

    for (auto& child : children)
        result->addChild(std::move(child));

    return result;
}

avg::Drawing drawNode(avg::RenderTreeNode const& node,
        std::chrono::milliseconds time = zero)
{
    avg::DrawContext context(pmr::new_delete_resource());

    return node.draw(context, avg::Obb(avg::Vector2f(400.0f, 400.0f)), time)
        .first;
}

avg::Drawing drawTree(avg::RenderTree const& tree,
        std::chrono::milliseconds time = zero)
{
    avg::DrawContext context(pmr::new_delete_resource());

    return tree.draw(context, avg::Obb(avg::Vector2f(400.0f, 400.0f)), time)
        .first;
}

/**
 * @brief One letter per element: S for a shape, E for an effect, [ ] around
 * the contents of a scope, ( ) around the contents of a clip, T for text, R
 * for a region.
 */
std::string structureOf(pmr::vector<avg::Drawing::Element> const& elements)
{
    std::string result;

    for (auto const& element : elements)
    {
        std::visit([&](auto const& e)
            {
                using T = std::decay_t<decltype(e)>;

                if constexpr (std::is_same_v<T, avg::Drawing::ShapeElement>)
                    result += 'S';
                else if constexpr (std::is_same_v<T, avg::Drawing::BackdropScope>)
                    result += '[' + structureOf(e.subDrawing->elements) + ']';
                else if constexpr (std::is_same_v<T, avg::Drawing::ClipElement>)
                    result += '(' + structureOf(e.subDrawing->elements) + ')';
                else if constexpr (std::is_same_v<T, avg::TextEntry>)
                    result += 'T';
                else if constexpr (std::is_same_v<T,
                        avg::Drawing::EffectElement>)
                    result += 'E';
                else
                    result += 'R';
            },
            element);
    }

    return result;
}

/**
 * @brief The left edge of every shape in a list, in order, entering scopes
 * and clips.
 */
std::vector<float> lefts(pmr::vector<avg::Drawing::Element> const& elements)
{
    std::vector<float> result;

    for (auto const& element : elements)
    {
        if (auto const* shape = std::get_if<avg::Drawing::ShapeElement>(
                    &element))
        {
            result.push_back(shape->shape.getControlBb().getLeft());
        }
        else if (auto const* scope = std::get_if<avg::Drawing::BackdropScope>(
                    &element))
        {
            auto inner = lefts(scope->subDrawing->elements);
            result.insert(result.end(), inner.begin(), inner.end());
        }
    }

    return result;
}

avg::Drawing::ShapeElement const& shapeAt(
        pmr::vector<avg::Drawing::Element> const& elements, size_t i)
{
    return std::get<avg::Drawing::ShapeElement>(elements.at(i));
}

avg::Drawing::BackdropScope const& scopeAt(
        pmr::vector<avg::Drawing::Element> const& elements, size_t i)
{
    return std::get<avg::Drawing::BackdropScope>(elements.at(i));
}

avg::Drawing::EffectElement const& effectAt(
        pmr::vector<avg::Drawing::Element> const& elements, size_t i)
{
    return std::get<avg::Drawing::EffectElement>(elements.at(i));
}

avg::Rect boundsOf(avg::Drawing::EffectElement const& effect)
{
    return (effect.transform * avg::Obb(effect.shape.getControlBb()))
        .getBoundingRect();
}

/**
 * @brief The parameter bytes of one pass of an effect.
 */
std::vector<char> parameterBytes(avg::Effect const& effect, size_t pass)
{
    auto const& parameters = effect.getPasses().at(pass).parameters;
    if (!parameters)
        return {};

    auto const* data = static_cast<char const*>(parameters->data());
    return std::vector<char>(data, data + parameters->getSize());
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

TEST(SurfaceNode, shadowIsPendingWhileFillAndChildAreContent)
{
    auto node = surface(placed(10.0f, 20.0f, 100.0f, 50.0f),
            shadowAt(5.0f, -5.0f),
            rect(placed(0.0f, 0.0f, 20.0f, 20.0f)));

    auto d = drawNode(*node);

    ASSERT_EQ("S", structureOf(d.getPendingShadows()));
    expectRect(shapeAt(d.getPendingShadows(), 0).shape.getControlBb(),
            15.0f, 15.0f, 100.0f, 50.0f);
    EXPECT_FALSE(shapeAt(d.getPendingShadows(), 0).pen.has_value());
    EXPECT_EQ(black, shapeAt(d.getPendingShadows(), 0).brush->getColor());

    ASSERT_EQ("SS", structureOf(d.getElements()));
    expectRect(shapeAt(d.getElements(), 0).shape.getControlBb(),
            10.0f, 20.0f, 100.0f, 50.0f);
    EXPECT_EQ(white, shapeAt(d.getElements(), 0).brush->getColor());
    expectRect(shapeAt(d.getElements(), 1).shape.getControlBb(),
            10.0f, 20.0f, 20.0f, 20.0f);
}

TEST(SurfaceNode, spreadGrowsTheShadowWithAPenOfTheShadowBrush)
{
    auto node = surface(placed(0.0f, 0.0f, 100.0f, 50.0f),
            shadowAt(0.0f, 0.0f, 4.0f));

    auto d = drawNode(*node);

    ASSERT_EQ(1u, d.getPendingShadows().size());
    auto const& shadow = shapeAt(d.getPendingShadows(), 0);
    ASSERT_TRUE(shadow.pen.has_value());
    EXPECT_FLOAT_EQ(8.0f, shadow.pen->getWidth());
    EXPECT_EQ(black, shadow.pen->getBrush().getColor());
}

TEST(SurfaceNode, withoutAShadowOrChildOnlyTheFillIsDrawn)
{
    auto node = surface(placed(0.0f, 0.0f, 100.0f, 50.0f), std::nullopt);

    auto d = drawNode(*node);

    EXPECT_EQ("", structureOf(d.getPendingShadows()));
    EXPECT_EQ("S", structureOf(d.getElements()));
}

TEST(SurfaceNode, snapshotReportsTheChild)
{
    avg::DrawContext context(pmr::new_delete_resource());

    auto node = surface(placed(10.0f, 20.0f, 100.0f, 50.0f),
            shadowAt(5.0f, -5.0f),
            rect(placed(0.0f, 0.0f, 20.0f, 20.0f)));

    auto snapshot = node->snapshot(context,
            avg::Obb(avg::Vector2f(400.0f, 400.0f)), zero);

    EXPECT_EQ("SurfaceNode", snapshot.type);
    ASSERT_EQ(1u, snapshot.children.size());
    EXPECT_EQ("RectNode", snapshot.children[0].type);
    expectRect(snapshot.children[0].obb.getBoundingRect(),
            10.0f, 20.0f, 20.0f, 20.0f);
}

TEST(StackingContainer, laterChildrenPaintOverEarlierOnesBehindTheirShadows)
{
    auto stack = container(avg::Obb(avg::Vector2f(400.0f, 400.0f)),
            avg::ContainerNode::Stacking::stacked, {
            surface(placed(0.0f, 0.0f, 100.0f, 50.0f), shadowAt(1.0f, 0.0f),
                    rect(placed(0.0f, 0.0f, 20.0f, 20.0f))),
            surface(placed(200.0f, 0.0f, 100.0f, 50.0f), shadowAt(2.0f, 0.0f),
                    rect(placed(0.0f, 0.0f, 20.0f, 20.0f)))
            });

    auto d = drawNode(*stack);

    // First surface flat, then the second's shadow, then its content scoped.
    EXPECT_EQ("SSS[SS]", structureOf(d.getElements()));
    EXPECT_EQ((std::vector<float>{ 0.0f, 0.0f, 202.0f, 200.0f, 200.0f }),
            lefts(d.getElements()));

    // The first surface's shadow waits for the enclosing container.
    EXPECT_EQ("S", structureOf(d.getPendingShadows()));
    EXPECT_EQ((std::vector<float>{ 1.0f }), lefts(d.getPendingShadows()));
}

TEST(StackingContainer, aNestedStackStaysInsideItsSiblingScope)
{
    auto inner = container(avg::Obb(avg::Vector2f(400.0f, 400.0f)),
            avg::ContainerNode::Stacking::stacked, {
            surface(placed(100.0f, 0.0f, 50.0f, 50.0f), shadowAt(1.0f, 0.0f)),
            surface(placed(200.0f, 0.0f, 50.0f, 50.0f), shadowAt(2.0f, 0.0f))
            });

    auto outer = container(avg::Obb(avg::Vector2f(400.0f, 400.0f)),
            avg::ContainerNode::Stacking::stacked, {
            surface(placed(0.0f, 0.0f, 50.0f, 50.0f), shadowAt(1.0f, 0.0f)),
            inner,
            surface(placed(300.0f, 0.0f, 50.0f, 50.0f), shadowAt(3.0f, 0.0f))
            });

    auto d = drawNode(*outer);

    EXPECT_EQ("SS[SS[S]]S[S]", structureOf(d.getElements()));
    EXPECT_EQ((std::vector<float>{
                0.0f,           // first surface, flat
                101.0f,         // the inner stack's pending shadow
                100.0f, 202.0f, 200.0f,  // the inner stack, scoped
                303.0f,         // the last surface's shadow
                300.0f          // the last surface, scoped
                }),
            lefts(d.getElements()));

    auto const& innerScope = scopeAt(d.getElements(), 2);
    EXPECT_EQ("SS[S]", structureOf(innerScope.subDrawing->elements));
    EXPECT_EQ("S", structureOf(scopeAt(d.getElements(), 4).subDrawing->elements));

    EXPECT_EQ((std::vector<float>{ 1.0f }), lefts(d.getPendingShadows()));
}

TEST(StackingContainer, aStackOfOneChildIsFlat)
{
    auto stack = container(avg::Obb(avg::Vector2f(400.0f, 400.0f)),
            avg::ContainerNode::Stacking::stacked, {
            surface(placed(0.0f, 0.0f, 100.0f, 50.0f), shadowAt(1.0f, 0.0f))
            });

    auto d = drawNode(*stack);

    EXPECT_EQ("S", structureOf(d.getElements()));
    EXPECT_EQ("S", structureOf(d.getPendingShadows()));
}

TEST(FlatContainer, nestedFlatContainersDrawLikeOneFlatContainer)
{
    auto nested = container(avg::Obb(avg::Vector2f(400.0f, 400.0f)),
            avg::ContainerNode::Stacking::flat, {
            container(avg::Obb(avg::Vector2f(400.0f, 400.0f)),
                avg::ContainerNode::Stacking::flat, {
                surface(placed(0.0f, 0.0f, 50.0f, 50.0f), shadowAt(1.0f, 0.0f)),
                rect(placed(100.0f, 0.0f, 50.0f, 50.0f))
                }),
            container(avg::Obb(avg::Vector2f(400.0f, 400.0f)),
                avg::ContainerNode::Stacking::flat, {
                surface(placed(200.0f, 0.0f, 50.0f, 50.0f), shadowAt(2.0f, 0.0f)),
                rect(placed(300.0f, 0.0f, 50.0f, 50.0f))
                })
            });

    auto flat = container(avg::Obb(avg::Vector2f(400.0f, 400.0f)),
            avg::ContainerNode::Stacking::flat, {
            surface(placed(0.0f, 0.0f, 50.0f, 50.0f), shadowAt(1.0f, 0.0f)),
            rect(placed(100.0f, 0.0f, 50.0f, 50.0f)),
            surface(placed(200.0f, 0.0f, 50.0f, 50.0f), shadowAt(2.0f, 0.0f)),
            rect(placed(300.0f, 0.0f, 50.0f, 50.0f))
            });

    auto a = drawNode(*nested);
    auto b = drawNode(*flat);

    EXPECT_EQ("SSSS", structureOf(a.getElements()));
    EXPECT_EQ(structureOf(b.getElements()), structureOf(a.getElements()));
    EXPECT_EQ(lefts(b.getElements()), lefts(a.getElements()));
    EXPECT_EQ("SS", structureOf(a.getPendingShadows()));
    EXPECT_EQ(lefts(b.getPendingShadows()), lefts(a.getPendingShadows()));
}

TEST(RenderTree, drawFlushesPendingShadowsAheadOfTheContent)
{
    auto stack = container(avg::Obb(avg::Vector2f(400.0f, 400.0f)),
            avg::ContainerNode::Stacking::stacked, {
            surface(placed(0.0f, 0.0f, 100.0f, 50.0f), shadowAt(1.0f, 0.0f)),
            surface(placed(200.0f, 0.0f, 100.0f, 50.0f), shadowAt(2.0f, 0.0f))
            });

    auto d = drawTree(avg::RenderTree(std::move(stack)));

    EXPECT_EQ("SSS[S]", structureOf(d.getElements()));
    EXPECT_EQ((std::vector<float>{ 1.0f, 0.0f, 202.0f, 200.0f }),
            lefts(d.getElements()));
    EXPECT_TRUE(d.getPendingShadows().empty());
}

TEST(SurfaceNode, animatingAShadowChangesValuesNotStructure)
{
    std::chrono::milliseconds const duration(1000);
    avg::AnimationOptions const options{ duration, avg::curve::linear };

    avg::RenderTree tree(surface(placed(0.0f, 0.0f, 100.0f, 50.0f),
                shadowAt(0.0f, 0.0f), rect(placed(0.0f, 0.0f, 20.0f, 20.0f))));

    avg::RenderTree next(surface(placed(0.0f, 0.0f, 100.0f, 50.0f),
                shadowAt(10.0f, 0.0f, 4.0f),
                rect(placed(0.0f, 0.0f, 20.0f, 20.0f))));

    auto [updated, nextUpdate] = std::move(tree).update(std::move(next),
            options, zero);

    auto start = drawTree(updated, zero);
    auto halfway = drawTree(updated, duration / 2);
    auto end = drawTree(updated, duration);

    EXPECT_EQ("SSS", structureOf(start.getElements()));
    EXPECT_EQ("SSS", structureOf(halfway.getElements()));
    EXPECT_EQ("SSS", structureOf(end.getElements()));

    EXPECT_FLOAT_EQ(0.0f, shapeAt(start.getElements(), 0).shape.getControlBb()
            .getLeft());
    EXPECT_FLOAT_EQ(5.0f, shapeAt(halfway.getElements(), 0).shape.getControlBb()
            .getLeft());
    EXPECT_FLOAT_EQ(10.0f, shapeAt(end.getElements(), 0).shape.getControlBb()
            .getLeft());

    ASSERT_TRUE(shapeAt(halfway.getElements(), 0).pen.has_value());
    EXPECT_FLOAT_EQ(4.0f, shapeAt(halfway.getElements(), 0).pen->getWidth());

    avg::DrawContext context(pmr::new_delete_resource());
    EXPECT_TRUE(updated.draw(context, avg::Obb(avg::Vector2f(400.0f, 400.0f)),
                duration / 2).second);
    EXPECT_FALSE(updated.draw(context, avg::Obb(avg::Vector2f(400.0f, 400.0f)),
                duration).second);
}

TEST(SurfaceNode, effectGoesAheadOfTheFillWhileTheShadowStaysPending)
{
    auto node = surface(placed(10.0f, 20.0f, 100.0f, 50.0f),
            shadowAt(5.0f, -5.0f),
            rect(placed(0.0f, 0.0f, 20.0f, 20.0f)),
            glass(8.0f));

    auto d = drawNode(*node);

    EXPECT_EQ("S", structureOf(d.getPendingShadows()));
    ASSERT_EQ("ESS", structureOf(d.getElements()));

    auto const& effect = effectAt(d.getElements(), 0);
    expectRect(boundsOf(effect), 10.0f, 20.0f, 100.0f, 50.0f);
    ASSERT_TRUE(effect.effect);
    EXPECT_EQ(4u, effect.effect->getPasses().size());
    expectRect(shapeAt(d.getElements(), 1).shape.getControlBb(),
            10.0f, 20.0f, 100.0f, 50.0f);
}

TEST(SurfaceNode, tintIsOneShapePass)
{
    auto node = surface(placed(0.0f, 0.0f, 100.0f, 50.0f), std::nullopt,
            nullptr, avg::SurfaceTint{ black, 0.5f });

    auto d = drawNode(*node);

    ASSERT_EQ("ES", structureOf(d.getElements()));
    EXPECT_EQ(1u, effectAt(d.getElements(), 0).effect->getPasses().size());
}

TEST(StackingContainer, aStackOfGlassSurfacesScopesEachLaterSurface)
{
    auto stack = container(avg::Obb(avg::Vector2f(400.0f, 400.0f)),
            avg::ContainerNode::Stacking::stacked, {
            surface(placed(0.0f, 0.0f, 100.0f, 50.0f), shadowAt(1.0f, 0.0f),
                    rect(placed(0.0f, 0.0f, 20.0f, 20.0f)), glass(8.0f)),
            surface(placed(200.0f, 0.0f, 100.0f, 50.0f), shadowAt(2.0f, 0.0f),
                    rect(placed(0.0f, 0.0f, 20.0f, 20.0f)), glass(8.0f))
            });

    auto d = drawNode(*stack);

    EXPECT_EQ("ESSS[ESS]", structureOf(d.getElements()));
    EXPECT_EQ((std::vector<float>{ 0.0f, 0.0f, 202.0f, 200.0f, 200.0f }),
            lefts(d.getElements()));
    expectRect(boundsOf(effectAt(d.getElements(), 0)),
            0.0f, 0.0f, 100.0f, 50.0f);
    expectRect(boundsOf(effectAt(
                    scopeAt(d.getElements(), 4).subDrawing->elements, 0)),
            200.0f, 0.0f, 100.0f, 50.0f);
    EXPECT_EQ("S", structureOf(d.getPendingShadows()));
}

TEST(SurfaceNode, animatingTheEffectChangesParametersNotStructure)
{
    std::chrono::milliseconds const duration(1000);
    avg::AnimationOptions const options{ duration, avg::curve::linear };

    avg::RenderTree tree(surface(placed(0.0f, 0.0f, 100.0f, 50.0f),
                shadowAt(0.0f, 0.0f), rect(placed(0.0f, 0.0f, 20.0f, 20.0f)),
                glass(8.0f)));

    avg::RenderTree next(surface(placed(0.0f, 0.0f, 100.0f, 50.0f),
                shadowAt(0.0f, 0.0f), rect(placed(0.0f, 0.0f, 20.0f, 20.0f)),
                glass(16.0f)));

    auto [updated, nextUpdate] = std::move(tree).update(std::move(next),
            options, zero);

    auto start = drawTree(updated, zero);
    auto halfway = drawTree(updated, duration / 2);
    auto end = drawTree(updated, duration);

    EXPECT_EQ("SESS", structureOf(start.getElements()));
    EXPECT_EQ("SESS", structureOf(halfway.getElements()));
    EXPECT_EQ("SESS", structureOf(end.getElements()));

    auto const& startEffect = *effectAt(start.getElements(), 1).effect;
    auto const& halfwayEffect = *effectAt(halfway.getElements(), 1).effect;
    auto const& endEffect = *effectAt(end.getElements(), 1).effect;

    ASSERT_EQ(startEffect.getPasses().size(), halfwayEffect.getPasses().size());
    for (size_t i = 0; i < startEffect.getPasses().size(); ++i)
    {
        EXPECT_EQ(startEffect.getPasses()[i].geometry,
                halfwayEffect.getPasses()[i].geometry);
        EXPECT_EQ(startEffect.getPasses()[i].shader.fragmentSource,
                halfwayEffect.getPasses()[i].shader.fragmentSource);
    }

    // The blur passes carry the radius.
    EXPECT_NE(parameterBytes(startEffect, 1), parameterBytes(halfwayEffect, 1));
    EXPECT_NE(parameterBytes(halfwayEffect, 1), parameterBytes(endEffect, 1));
    EXPECT_EQ(parameterBytes(startEffect, 3), parameterBytes(halfwayEffect, 3));

    avg::DrawContext context(pmr::new_delete_resource());
    EXPECT_TRUE(updated.draw(context, avg::Obb(avg::Vector2f(400.0f, 400.0f)),
                duration / 2).second);
    EXPECT_FALSE(updated.draw(context, avg::Obb(avg::Vector2f(400.0f, 400.0f)),
                duration).second);
}

TEST(SurfaceNode, anOvershootingCurveDrawsTheEffectClamped)
{
    std::chrono::milliseconds const duration(1000);
    ASSERT_GT(avg::curve::easeOutBack(0.6f), 1.0f);
    ASSERT_LT(avg::curve::easeInBack(0.4f), 0.0f);

    auto tinted = [](float strength)
    {
        return surface(placed(0.0f, 0.0f, 100.0f, 50.0f), std::nullopt,
                nullptr, avg::SurfaceTint{ black, strength });
    };

    auto glassy = [](float radius)
    {
        return surface(placed(0.0f, 0.0f, 100.0f, 50.0f), std::nullopt,
                nullptr, glass(radius));
    };

    auto [past, pastUpdate] = avg::RenderTree(tinted(0.0f)).update(
            avg::RenderTree(tinted(1.0f)),
            avg::AnimationOptions{ duration, avg::curve::easeOutBack }, zero);
    auto [below, belowUpdate] = avg::RenderTree(glassy(0.0f)).update(
            avg::RenderTree(glassy(8.0f)),
            avg::AnimationOptions{ duration, avg::curve::easeInBack }, zero);

    for (int i = 0; i <= 10; ++i)
    {
        EXPECT_NO_THROW(drawTree(past, duration * i / 10)) << "step " << i;
        EXPECT_NO_THROW(drawTree(below, duration * i / 10)) << "step " << i;
    }

    auto overshot = drawTree(past, duration * 6 / 10);
    auto end = drawTree(past, duration);
    EXPECT_EQ(parameterBytes(*effectAt(end.getElements(), 0).effect, 0),
            parameterBytes(*effectAt(overshot.getElements(), 0).effect, 0));
}

TEST(SurfaceNode, frostedGlassOptionsInterpolateMemberwise)
{
    avg::FrostedGlassOptions a;
    a.radius = 8.0f;
    a.tintStrength = 0.0f;
    a.tint = black;

    avg::FrostedGlassOptions b;
    b.radius = 16.0f;
    b.tintStrength = 1.0f;
    b.tint = white;

    auto mid = avg::lerp(a, b, 0.5f);
    EXPECT_FLOAT_EQ(12.0f, mid.radius);
    EXPECT_FLOAT_EQ(0.5f, mid.tintStrength);
    EXPECT_FLOAT_EQ(0.5f, mid.tint.getRed());
    EXPECT_FLOAT_EQ(a.saturation, mid.saturation);

    EXPECT_TRUE(a == avg::lerp(a, b, 0.0f));
    EXPECT_TRUE(a != b);
}
