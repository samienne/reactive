#include <avg/rendering.h>
#include <avg/painter.h>
#include <avg/drawing.h>
#include <avg/drawcontext.h>
#include <avg/targetimage.h>
#include <avg/builtineffects.h>
#include <avg/effect.h>
#include <avg/effectplan.h>
#include <avg/brush.h>
#include <avg/color.h>
#include <avg/obb.h>
#include <avg/pathbuilder.h>
#include <avg/rect.h>
#include <avg/rendertree.h>
#include <avg/shape.h>
#include <avg/shapefunction.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <ase/commandbuffer.h>
#include <ase/rendercommand.h>
#include <ase/drawcommand.h>
#include <ase/dummyplatform.h>
#include <ase/dummyrenderqueue.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>
#include <ase/renderqueue.h>
#include <ase/format.h>
#include <ase/texture.h>
#include <ase/vector.h>

#include <btl/runloop.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

using avg::PixelRect;

namespace
{
    avg::Vector2i const screenSize(400, 300);
    avg::Color const white(1.0f, 1.0f, 1.0f, 1.0f);
    avg::Color const black(0.0f, 0.0f, 0.0f, 1.0f);
    PixelRect const glassBounds{ 100, 80, 200, 40 };

    avg::Shape box(float x, float y, float width, float height)
    {
        return avg::Shape(avg::PathBuilder(pmr::new_delete_resource())
                .start(x, y)
                .lineTo(x + width, y)
                .lineTo(x + width, y + height)
                .lineTo(x, y + height)
                .close()
                .build());
    }

    avg::Shape surfaceBox(avg::DrawContext const& context, avg::Vector2f size)
    {
        return avg::Shape(context.pathBuilder()
                .start(0.0f, 0.0f)
                .lineTo(size[0], 0.0f)
                .lineTo(size[0], size[1])
                .lineTo(0.0f, size[1])
                .close()
                .build());
    }

    avg::Drawing::Element fill(float x, float y, float width, float height)
    {
        return avg::Drawing::ShapeElement{ box(x, y, width, height),
            avg::Brush(white), std::nullopt };
    }

    std::shared_ptr<avg::Effect const> glass(float radius = 24.0f)
    {
        avg::FrostedGlassOptions options;
        options.radius = radius;
        return std::make_shared<avg::Effect const>(avg::frostedGlass(options));
    }

    avg::Drawing::Element effect(PixelRect const& bounds,
            std::shared_ptr<avg::Effect const> effect = glass())
    {
        return avg::Drawing::EffectElement{
            box(static_cast<float>(bounds.x), static_cast<float>(bounds.y),
                    static_cast<float>(bounds.width),
                    static_cast<float>(bounds.height)),
            std::move(effect),
            avg::Transform()
        };
    }

    avg::Drawing drawing(std::vector<avg::Drawing::Element> elements)
    {
        avg::Drawing result(pmr::new_delete_resource());
        for (auto& element : elements)
            result += std::move(element);

        return result;
    }

    avg::Drawing::Element scope(std::vector<avg::Drawing::Element> elements)
    {
        auto scoped = drawing(std::move(elements)).scope();
        EXPECT_EQ(1u, scoped.getElements().size());
        return scoped.getElements().front();
    }

    avg::EffectRegions regionsOf(avg::Effect const& effect,
            PixelRect const& bounds)
    {
        return avg::deriveRegions(effect, bounds, screenSize);
    }

    struct Fixture
    {
        btl::RunLoop loop;
        ase::Platform platform = ase::makeDummyPlatform(loop);
        ase::RenderContext context = platform.makeRenderContext();
        avg::Painter painter{ pmr::new_delete_resource(), context };
        avg::TargetImage target{ context, screenSize, ase::FORMAT_SRGBA };

        // The draw buffer of one image frame: clear, paint and flush
        ase::CommandBuffer const& frame(avg::Drawing const& drawing,
                float scalingFactor = 1.0f)
        {
            auto& queue = context.getMainRenderQueue()
                .getImpl<ase::DummyRenderQueue>();
            queue.recordSubmissions(true);
            size_t const before = queue.getSubmissions().size();

            painter.clearImage(target);
            painter.paintToImage(target, scalingFactor, drawing);
            painter.flush();

            EXPECT_EQ(before + 2, queue.getSubmissions().size());
            return queue.getSubmissions().back();
        }

        ase::CommandBuffer render(avg::Drawing const& drawing)
        {
            ase::CommandBuffer commands;
            avg::render(pmr::new_delete_resource(), commands, context, target,
                    1.0f, painter, drawing);

            return commands;
        }

        bool isMain(ase::DrawCommand const& draw)
        {
            return draw.getFramebuffer() == target.getFramebuffer();
        }

        bool isContent(ase::DrawCommand const& draw)
        {
            return isMain(draw)
                && draw.getPipeline() == painter.getPipeline(avg::Brush(white));
        }

        bool isSnapshot(ase::DrawCommand const& draw)
        {
            return draw.getTextures().size() == 1
                && draw.getTextures()[0] == target.getTexture();
        }

        // One letter per command: C clear, | barrier, D content draw, S
        // snapshot, P pass into scratch, F final pass into the target.
        std::string picture(ase::CommandBuffer const& commands)
        {
            std::string result;
            for (auto const& command : commands)
            {
                if (std::holds_alternative<ase::ClearCommand>(command))
                    result += 'C';
                else if (std::holds_alternative<ase::BarrierCommand>(command))
                    result += '|';
                else if (auto const* draw = std::get_if<ase::DrawCommand>(
                            &command))
                {
                    if (isSnapshot(*draw))
                        result += 'S';
                    else if (!isMain(*draw))
                        result += 'P';
                    else if (isContent(*draw))
                        result += 'D';
                    else
                        result += 'F';
                }
            }

            return result;
        }

        // The draws of commands in order, picture letters as above
        std::vector<ase::DrawCommand const*> draws(
                ase::CommandBuffer const& commands, std::string const& kinds)
        {
            std::vector<ase::DrawCommand const*> result;
            std::string const all = picture(commands);
            size_t i = 0;
            for (auto const& command : commands)
            {
                auto const* draw = std::get_if<ase::DrawCommand>(&command);
                if (!draw)
                    continue;

                while (all[i] == 'C' || all[i] == '|')
                    ++i;

                if (kinds.find(all[i]) != std::string::npos)
                    result.push_back(draw);

                ++i;
            }

            return result;
        }
    };

    // The vertex data uploaded for draw
    std::vector<float> verticesOf(ase::CommandBuffer const& commands,
            ase::DrawCommand const& draw)
    {
        std::vector<float> vertices;
        for (auto const& candidate : commands)
        {
            auto const* upload = std::get_if<ase::BufferUploadCommand>(
                    &candidate);
            if (!upload)
                continue;

            auto const* vb = std::get_if<ase::VertexBuffer>(&upload->target);
            if (!vb || *vb != draw.getVertexBuffer())
                continue;

            float const* data = upload->data.mapRead<float>();
            vertices.assign(data,
                    data + upload->data.getSize() / sizeof(float));
        }

        return vertices;
    }

    // The least and greatest x of vertices of stride floats, x at offset
    std::pair<float, float> xRange(std::vector<float> const& vertices,
            size_t stride, size_t offset)
    {
        float low = 0.0f;
        float high = 0.0f;
        for (size_t i = offset; i < vertices.size(); i += stride)
        {
            if (i == offset || vertices[i] < low)
                low = vertices[i];
            if (i == offset || vertices[i] > high)
                high = vertices[i];
        }

        return { low, high };
    }

    void expectScissor(PixelRect const& expected, ase::DrawCommand const& draw)
    {
        ASSERT_TRUE(draw.getScissor().has_value());
        EXPECT_EQ(expected.x, draw.getScissor()->x);
        EXPECT_EQ(expected.y, draw.getScissor()->y);
        EXPECT_EQ(expected.width, draw.getScissor()->width);
        EXPECT_EQ(expected.height, draw.getScissor()->height);
    }

    bool hasBarrier(ase::CommandBuffer const& commands)
    {
        return std::any_of(commands.begin(), commands.end(),
                [](ase::RenderCommand const& c)
                {
                    return std::holds_alternative<ase::BarrierCommand>(c);
                });
    }
} // anonymous namespace

TEST(EffectRendering, aScopeWithoutEffectsAddsNoCommands)
{
    Fixture f;
    auto flat = drawing({ fill(0.0f, 0.0f, 50.0f, 50.0f),
            fill(60.0f, 0.0f, 50.0f, 50.0f),
            fill(120.0f, 0.0f, 50.0f, 50.0f) });
    auto scoped = drawing({ fill(0.0f, 0.0f, 50.0f, 50.0f),
            scope({ fill(60.0f, 0.0f, 50.0f, 50.0f),
                    scope({ fill(120.0f, 0.0f, 50.0f, 50.0f) }) }) });

    auto expectedCommands = f.render(flat);
    auto actualCommands = f.render(scoped);

    EXPECT_FALSE(hasBarrier(actualCommands));
    EXPECT_EQ(f.picture(expectedCommands), f.picture(actualCommands));

    auto expected = f.draws(expectedCommands, "D");
    auto actual = f.draws(actualCommands, "D");
    ASSERT_EQ(1u, expected.size());
    ASSERT_EQ(1u, actual.size());
    EXPECT_FLOAT_EQ(expected[0]->getZ(), actual[0]->getZ());
    EXPECT_EQ(verticesOf(expectedCommands, *expected[0]),
            verticesOf(actualCommands, *actual[0]));
}

TEST(EffectRendering, anEffectInAScopeSnapshotsWhatWasPaintedBeforeTheScope)
{
    Fixture f;
    auto glassEffect = glass();
    auto d = drawing({ fill(0.0f, 0.0f, 50.0f, 50.0f),
            scope({ effect(glassBounds, glassEffect),
                    fill(300.0f, 0.0f, 50.0f, 50.0f) }) });

    auto const& frame = f.frame(d);
    EXPECT_EQ("CD|S|P|P|P|F|D", f.picture(frame));

    avg::EffectRegions regions = regionsOf(*glassEffect, glassBounds);
    ASSERT_EQ(4u, regions.passRegions.size());

    auto snapshots = f.draws(frame, "S");
    ASSERT_EQ(1u, snapshots.size());
    EXPECT_FALSE(f.isMain(*snapshots[0]));
    expectScissor(regions.backdropRegion, *snapshots[0]);

    auto passes = f.draws(frame, "PF");
    ASSERT_EQ(4u, passes.size());
    for (size_t i = 0; i < passes.size(); ++i)
        expectScissor(regions.passRegions[i], *passes[i]);

    // The final pass fills the shape, in target pixels, over the target
    EXPECT_TRUE(f.isMain(*passes[3]));
    EXPECT_TRUE(passes[3]->getPipeline().isBlendEnabled());
    EXPECT_FALSE(passes[3]->getPipeline().isDepthTestEnabled());
    auto triangles = verticesOf(frame, *passes[3]);
    ASSERT_GE(triangles.size(), 12u);
    EXPECT_EQ(0u, triangles.size() % 6);
    EXPECT_EQ(std::make_pair(100.0f, 300.0f), xRange(triangles, 2, 0));
    EXPECT_EQ(std::make_pair(80.0f, 120.0f), xRange(triangles, 2, 1));

    // Content before the scope is drawn before the snapshot, content after
    // the effect after its final pass
    auto content = f.draws(frame, "D");
    ASSERT_EQ(2u, content.size());
    EXPECT_EQ(std::make_pair(0.0f, 50.0f),
            xRange(verticesOf(frame, *content[0]), 7, 4));
    EXPECT_EQ(std::make_pair(300.0f, 350.0f),
            xRange(verticesOf(frame, *content[1]), 7, 4));
    EXPECT_GT(content[0]->getZ(), content[1]->getZ());
}

TEST(EffectRendering, twoEffectsInOneScopeShareOneSnapshot)
{
    Fixture f;
    auto glassEffect = glass();
    PixelRect const other{ 20, 20, 60, 60 };
    auto d = drawing({ scope({ effect(glassBounds, glassEffect),
            effect(other, glassEffect) }) });

    auto const& frame = f.frame(d);
    EXPECT_EQ("C|S|P|P|P|F|P|P|P|F|", f.picture(frame));

    auto snapshots = f.draws(frame, "S");
    ASSERT_EQ(1u, snapshots.size());
    expectScissor(regionsOf(*glassEffect, glassBounds).backdropRegion
            .unionWith(regionsOf(*glassEffect, other).backdropRegion),
            *snapshots[0]);

    // Both copy passes read the one snapshot texture
    auto passes = f.draws(frame, "P");
    ASSERT_EQ(6u, passes.size());
    ASSERT_EQ(1u, passes[0]->getTextures().size());
    ASSERT_EQ(1u, passes[3]->getTextures().size());
    EXPECT_EQ(passes[0]->getTextures()[0], passes[3]->getTextures()[0]);
    EXPECT_NE(passes[0]->getTextures()[0], passes[1]->getTextures()[0]);

    auto finals = f.draws(frame, "F");
    ASSERT_EQ(2u, finals.size());
    expectScissor(glassBounds, *finals[0]);
    expectScissor(other, *finals[1]);
}

TEST(EffectRendering, nestedScopesSnapshotAtTheInnermostScope)
{
    Fixture f;
    auto d = drawing({ fill(0.0f, 0.0f, 50.0f, 50.0f),
            scope({ fill(60.0f, 0.0f, 50.0f, 50.0f),
                    scope({ effect(glassBounds) }),
                    fill(300.0f, 0.0f, 50.0f, 50.0f) }) });

    auto const& frame = f.frame(d);
    EXPECT_EQ("CD|S|P|P|P|F|D", f.picture(frame));

    // Everything before the inner scope, inside the outer one or not, is
    // drawn before the one snapshot
    auto content = f.draws(frame, "D");
    ASSERT_EQ(2u, content.size());
    EXPECT_EQ(std::make_pair(0.0f, 110.0f),
            xRange(verticesOf(frame, *content[0]), 7, 4));
    EXPECT_EQ(std::make_pair(300.0f, 350.0f),
            xRange(verticesOf(frame, *content[1]), 7, 4));
}

TEST(EffectRendering, anEffectOutsideAScopeSnapshotsTheTargetAtTheRoot)
{
    Fixture f;
    auto d = drawing({ fill(0.0f, 0.0f, 50.0f, 50.0f), effect(glassBounds),
            fill(300.0f, 0.0f, 50.0f, 50.0f) });

    auto const& frame = f.frame(d);
    EXPECT_EQ("C|S|D|P|P|P|F|D", f.picture(frame));

    auto content = f.draws(frame, "D");
    ASSERT_EQ(2u, content.size());
    EXPECT_EQ(std::make_pair(0.0f, 50.0f),
            xRange(verticesOf(frame, *content[0]), 7, 4));
    EXPECT_EQ(std::make_pair(300.0f, 350.0f),
            xRange(verticesOf(frame, *content[1]), 7, 4));
}

TEST(EffectRendering, aClipCutsTheEffectsRegions)
{
    Fixture f;
    auto glassEffect = glass();
    auto d = drawing({ effect(glassBounds, glassEffect) })
        .clip(avg::Rect(avg::Vector2f(0.0f, 0.0f),
                    avg::Vector2f(200.0f, 300.0f)));

    auto const& frame = f.frame(d);
    EXPECT_EQ("C|S|P|P|P|F|", f.picture(frame));

    PixelRect const clipped{ 100, 80, 100, 40 };
    avg::EffectRegions regions = regionsOf(*glassEffect, clipped);

    auto snapshots = f.draws(frame, "S");
    ASSERT_EQ(1u, snapshots.size());
    expectScissor(regions.backdropRegion, *snapshots[0]);

    auto passes = f.draws(frame, "PF");
    ASSERT_EQ(4u, passes.size());
    for (size_t i = 0; i < passes.size(); ++i)
        expectScissor(regions.passRegions[i], *passes[i]);
}

TEST(EffectRendering, anEffectOffTheTargetDrawsNothing)
{
    Fixture f;
    auto d = drawing({ effect(PixelRect{ 500, 500, 20, 20 }) });

    auto const& frame = f.frame(d);
    EXPECT_EQ("C", f.picture(frame));
}

TEST(EffectRendering, anEffectWithoutAreaDrawsNothing)
{
    Fixture f;
    auto d = drawing({ fill(0.0f, 0.0f, 50.0f, 50.0f),
            scope({ avg::Drawing::EffectElement{
                    box(100.5f, 80.0f, 0.0f, 40.0f), glass(),
                    avg::Transform() } }) });

    auto const& frame = f.frame(d);
    EXPECT_EQ("CD", f.picture(frame));
}

TEST(EffectRendering, anEffectWithHugeBoundsIsClampedToTheTarget)
{
    Fixture f;
    auto d = drawing({ avg::Drawing::EffectElement{
            box(-1.0e7f, -1.0e7f, 2.0e7f, 2.0e7f), glass(),
            avg::Transform() } });

    // The bounds are beyond the range of an int in target pixels
    auto const& frame = f.frame(d, 1000.0f);
    EXPECT_EQ("C|S|P|P|P|F|", f.picture(frame));

    auto finals = f.draws(frame, "F");
    ASSERT_EQ(1u, finals.size());
    expectScissor(PixelRect{ 0, 0, 400, 300 },
            *finals[0]);
}

TEST(EffectRendering, aScaledFrameCullsInLogicalUnits)
{
    Fixture f;
    auto inside = drawing({ fill(450.0f, 0.0f, 50.0f, 50.0f) });
    auto outside = drawing({ fill(250.0f, 0.0f, 50.0f, 50.0f) });

    // The 400 pixel wide target is 800 units wide at a factor of 0.5 and
    // 200 at 2
    EXPECT_EQ("CD", f.picture(f.frame(inside, 0.5f)));
    EXPECT_EQ("C", f.picture(f.frame(outside, 2.0f)));
}

TEST(EffectRendering, aScaledFramePlacesTheEffectInTargetPixels)
{
    Fixture f;
    auto glassEffect = glass();
    auto d = drawing({ effect(PixelRect{ 50, 40, 100, 20 }, glassEffect) });

    auto const& frame = f.frame(d, 2.0f);
    EXPECT_EQ("C|S|P|P|P|F|", f.picture(frame));

    avg::EffectRegions regions = regionsOf(*glassEffect, glassBounds);
    auto snapshots = f.draws(frame, "S");
    ASSERT_EQ(1u, snapshots.size());
    expectScissor(regions.backdropRegion, *snapshots[0]);

    auto finals = f.draws(frame, "F");
    ASSERT_EQ(1u, finals.size());
    expectScissor(glassBounds, *finals[0]);
    auto triangles = verticesOf(frame, *finals[0]);
    EXPECT_EQ(std::make_pair(100.0f, 300.0f), xRange(triangles, 2, 0));
    EXPECT_EQ(std::make_pair(80.0f, 120.0f), xRange(triangles, 2, 1));
}

TEST(EffectRendering, effectsInAnOuterAndAnInnerScopeKeepTheirSnapshots)
{
    Fixture f;
    auto glassEffect = glass();
    PixelRect const inner{ 20, 20, 60, 60 };
    PixelRect const last{ 20, 200, 60, 60 };
    auto d = drawing({ scope({ effect(glassBounds, glassEffect),
            scope({ effect(inner, glassEffect) }),
            effect(last, glassEffect) }) });

    auto const& frame = f.frame(d);
    EXPECT_EQ("C|S|P|P|P|F|S|P|P|P|F|P|P|P|F|", f.picture(frame));

    auto snapshots = f.draws(frame, "S");
    ASSERT_EQ(2u, snapshots.size());
    EXPECT_NE(snapshots[0]->getFramebuffer(), snapshots[1]->getFramebuffer());
    expectScissor(regionsOf(*glassEffect, glassBounds).backdropRegion
            .unionWith(regionsOf(*glassEffect, last).backdropRegion),
            *snapshots[0]);
    expectScissor(regionsOf(*glassEffect, inner).backdropRegion,
            *snapshots[1]);

    // The outer scope's two copy passes read one snapshot texture, the
    // inner scope's another
    auto passes = f.draws(frame, "P");
    ASSERT_EQ(9u, passes.size());
    ASSERT_EQ(1u, passes[0]->getTextures().size());
    ASSERT_EQ(1u, passes[3]->getTextures().size());
    ASSERT_EQ(1u, passes[6]->getTextures().size());
    EXPECT_EQ(passes[0]->getTextures()[0], passes[6]->getTextures()[0]);
    EXPECT_NE(passes[0]->getTextures()[0], passes[3]->getTextures()[0]);

    // Nothing draws over the outer snapshot while the last effect is
    // still to read it
    for (auto const* draw : f.draws(frame, "SPF"))
    {
        if (draw != snapshots[0])
        {
            EXPECT_NE(snapshots[0]->getFramebuffer(),
                    draw->getFramebuffer());
        }
    }
}

TEST(EffectRendering, aGlassSurfaceInAStackDrawsOverTheChildrenBelowIt)
{
    Fixture f;

    auto stack = std::make_shared<avg::ContainerNode>(
            avg::Obb(avg::Vector2f(400.0f, 300.0f)),
            avg::ContainerNode::Stacking::stacked);
    stack->addChild(std::make_shared<avg::RectNode>(
                avg::Obb(avg::Vector2f(400.0f, 300.0f)),
                std::nullopt,
                0.0f,
                std::make_optional(avg::Brush(white)),
                std::nullopt));

    avg::FrostedGlassOptions options;
    options.radius = 24.0f;
    stack->addChild(std::make_shared<avg::SurfaceNode>(
                avg::Obb(avg::Vector2f(200.0f, 40.0f),
                    avg::translate(100.0f, 80.0f)),
                std::nullopt,
                avg::makeShapeFunction(&surfaceBox),
                std::make_optional(avg::Brush(white)),
                std::make_optional(avg::SurfaceShadow{
                        avg::Vector2f(2.0f, -2.0f), 0.0f, avg::Brush(black) }),
                std::make_optional(avg::SurfaceEffect(
                        avg::SurfaceFrostedGlass{ options })),
                std::make_shared<avg::RectNode>(
                    avg::Obb(avg::Vector2f(20.0f, 20.0f)),
                    std::nullopt,
                    0.0f,
                    std::make_optional(avg::Brush(white)),
                    std::nullopt)));

    avg::RenderTree tree(std::move(stack));
    avg::DrawContext context(pmr::new_delete_resource());
    auto d = tree.draw(context, avg::Obb(avg::Vector2f(400.0f, 300.0f)),
            std::chrono::milliseconds(0)).first;
    ASSERT_TRUE(d.getPendingShadows().empty());

    // The shadow and the background, then the surface's snapshot, its
    // passes and its fill with the child over it
    auto const& frame = f.frame(d);
    EXPECT_EQ("CD|S|P|P|P|F|D", f.picture(frame));

    auto finals = f.draws(frame, "F");
    ASSERT_EQ(1u, finals.size());
    expectScissor(glassBounds, *finals[0]);

    // The shadow's black is in the draw before the snapshot, not in the
    // one after the final pass
    auto content = f.draws(frame, "D");
    ASSERT_EQ(2u, content.size());
    EXPECT_EQ(std::make_pair(0.0f, 1.0f),
            xRange(verticesOf(frame, *content[0]), 7, 0));
    EXPECT_EQ(std::make_pair(1.0f, 1.0f),
            xRange(verticesOf(frame, *content[1]), 7, 0));
}
