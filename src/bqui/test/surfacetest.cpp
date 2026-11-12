#include <bqui/modifier/ondraw.h>
#include <bqui/modifier/setsizehint.h>
#include <bqui/modifier/surface.h>

#include <bqui/widget/stack.h>
#include <bqui/widget/vbox.h>
#include <bqui/widget/widget.h>

#include <bqui/shape/rectangle.h>

#include <bqui/buildparams.h>
#include <bqui/simplesizehint.h>
#include <bqui/sizehint.h>

#include <bq/signal/constant.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <avg/brush.h>
#include <avg/builtineffects.h>
#include <avg/color.h>
#include <avg/drawcontext.h>
#include <avg/drawing.h>
#include <avg/obb.h>
#include <avg/rendertree.h>
#include <avg/rendertree/containernode.h>
#include <avg/rendertree/surfacenode.h>
#include <avg/vector.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

using namespace bqui;
using namespace bqui::widget;

namespace
{

using Milliseconds = std::chrono::milliseconds;
using Elements = pmr::vector<avg::Drawing::Element>;

avg::Color const red(1.0f, 0.0f, 0.0f, 1.0f);
avg::Color const green(0.0f, 1.0f, 0.0f, 1.0f);
avg::Color const blue(0.0f, 0.0f, 1.0f, 1.0f);
avg::Color const black(0.0f, 0.0f, 0.0f, 1.0f);

avg::Vector2f const size(100.0f, 50.0f);
Milliseconds const now(0);

SizeHintResult const fixed50 = {{ 50.0f, 50.0f, 50.0f }};
SizeHintResult const fixed100 = {{ 100.0f, 100.0f, 100.0f }};

avg::Drawing fillArea(avg::DrawContext const& context, avg::Vector2f size,
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
 * @brief A widget that fills whatever it is given with one color.
 */
AnyWidget probe(avg::Color const& color)
{
    return makeWidget()
        | modifier::setSizeHint(simpleSizeHint(fixed100, fixed50))
        | modifier::onDraw(&fillArea, bq::signal::constant(color))
        ;
}

Instance realise(AnyWidget widget)
{
    auto context = bq::signal::makeSignalContext(
            std::move(widget)(BuildParams())(
                bq::signal::constant(size)).getInstance());

    return context.evaluate<0>().get<0>();
}

/**
 * @brief Draws the realised widget the way the window loop does, pending
 * shadows flushed ahead of the content.
 */
avg::Drawing draw(Instance const& instance)
{
    avg::DrawContext context(pmr::new_delete_resource());

    return instance.getRenderTree().draw(context, avg::Obb(size), now).first;
}

template <typename T>
T const* elementAs(Elements const& elements, size_t index)
{
    if (index >= elements.size())
        return nullptr;

    return std::get_if<T>(&elements[index]);
}

std::optional<avg::Color> fillColorAt(Elements const& elements, size_t index)
{
    auto const* shape = elementAs<avg::Drawing::ShapeElement>(elements, index);

    if (!shape || !shape->brush)
        return std::nullopt;

    return shape->brush->getColor();
}

Elements const* scopedElementsAt(Elements const& elements, size_t index)
{
    auto const* scope = elementAs<avg::Drawing::BackdropScope>(elements,
            index);

    return scope ? &scope->subDrawing->elements : nullptr;
}

avg::SurfaceNode const* surfaceRootOf(Instance const& instance)
{
    return dynamic_cast<avg::SurfaceNode const*>(
            instance.getRenderTree().getRoot().get());
}

avg::FrostedGlassOptions glassOptions()
{
    avg::FrostedGlassOptions options;
    options.radius = 32.0f;
    return options;
}

} // anonymous namespace

TEST(Surface, surfaceYieldsASurfaceNodeWithWhatItWasGiven)
{
    using bq::signal::constant;

    auto instance = realise(probe(green)
            | modifier::surface(
                shape::rectangle(),
                constant(avg::Brush(red)),
                modifier::SurfaceShadow{
                    constant(avg::Vector2f(2.0f, -3.0f)),
                    constant(4.0f),
                    constant(avg::Brush(black))
                },
                modifier::SurfaceFrostedGlass{ constant(glassOptions()) }
                ));

    auto const* node = surfaceRootOf(instance);
    ASSERT_TRUE(node);

    ASSERT_TRUE(node->getFill().getValue(now).has_value());
    EXPECT_EQ(red, node->getFill().getValue(now)->getColor());

    ASSERT_TRUE(node->getShadow().has_value());
    EXPECT_EQ(avg::Vector2f(2.0f, -3.0f),
            node->getShadow()->offset.getValue(now));
    EXPECT_FLOAT_EQ(4.0f, node->getShadow()->spread.getValue(now));
    EXPECT_EQ(black, node->getShadow()->brush.getValue(now).getColor());

    ASSERT_TRUE(node->getEffect().has_value());
    auto const* glass = std::get_if<avg::SurfaceFrostedGlass>(
            &*node->getEffect());
    ASSERT_TRUE(glass);
    EXPECT_FLOAT_EQ(32.0f, glass->options.getValue(now).radius);

    ASSERT_TRUE(node->getChild());
}

TEST(Surface, surfaceDrawsShadowThenEffectThenFillThenChild)
{
    using bq::signal::constant;

    auto drawing = draw(realise(probe(green)
            | modifier::surface(
                shape::rectangle(),
                constant(avg::Brush(red)),
                modifier::SurfaceShadow{
                    constant(avg::Vector2f(2.0f, -3.0f)),
                    constant(4.0f),
                    constant(avg::Brush(black))
                },
                modifier::SurfaceFrostedGlass{ constant(glassOptions()) }
                )));

    auto const& elements = drawing.getElements();
    ASSERT_EQ(4u, elements.size());
    EXPECT_TRUE(drawing.getPendingShadows().empty());

    EXPECT_EQ(black, fillColorAt(elements, 0));
    EXPECT_TRUE(elementAs<avg::Drawing::EffectElement>(elements, 1));
    EXPECT_EQ(red, fillColorAt(elements, 2));
    EXPECT_EQ(green, fillColorAt(elements, 3));
}

TEST(Surface, frostedGlassSetsAFrostedGlassEffectAndNothingElse)
{
    auto instance = realise(probe(green)
            | modifier::frostedGlass(glassOptions()));

    auto const* node = surfaceRootOf(instance);
    ASSERT_TRUE(node);

    EXPECT_FALSE(node->getFill().getValue(now).has_value());
    EXPECT_FALSE(node->getShadow().has_value());

    ASSERT_TRUE(node->getEffect().has_value());
    auto const* glass = std::get_if<avg::SurfaceFrostedGlass>(
            &*node->getEffect());
    ASSERT_TRUE(glass);
    EXPECT_FLOAT_EQ(32.0f, glass->options.getValue(now).radius);
}

TEST(Surface, tintSetsATintEffect)
{
    using bq::signal::constant;

    auto instance = realise(probe(green)
            | modifier::tint(constant(blue), constant(0.25f)));

    auto const* node = surfaceRootOf(instance);
    ASSERT_TRUE(node);

    ASSERT_TRUE(node->getEffect().has_value());
    auto const* tint = std::get_if<avg::SurfaceTint>(&*node->getEffect());
    ASSERT_TRUE(tint);
    EXPECT_EQ(blue, tint->colour.getValue(now));
    EXPECT_FLOAT_EQ(0.25f, tint->strength.getValue(now));
}

TEST(Surface, stackPaintsEachLaterChildInItsOwnBackdropScope)
{
    auto drawing = draw(realise(stack({
                    probe(red),
                    probe(green),
                    probe(blue)
                    })));

    auto const& elements = drawing.getElements();
    ASSERT_EQ(3u, elements.size());

    EXPECT_EQ(red, fillColorAt(elements, 0));

    auto const* second = scopedElementsAt(elements, 1);
    ASSERT_TRUE(second);
    ASSERT_EQ(1u, second->size());
    EXPECT_EQ(green, fillColorAt(*second, 0));

    auto const* third = scopedElementsAt(elements, 2);
    ASSERT_TRUE(third);
    ASSERT_EQ(1u, third->size());
    EXPECT_EQ(blue, fillColorAt(*third, 0));
}

TEST(Surface, vboxPaintsItsChildrenFlat)
{
    auto drawing = draw(realise(vbox({
                    probe(red),
                    probe(green)
                    })));

    auto const& elements = drawing.getElements();
    ASSERT_EQ(2u, elements.size());

    EXPECT_EQ(red, fillColorAt(elements, 0));
    EXPECT_EQ(green, fillColorAt(elements, 1));
}
