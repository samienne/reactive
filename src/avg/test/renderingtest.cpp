#include <glyphatlas.h>

#include <avg/rendering.h>
#include <avg/painter.h>
#include <avg/drawing.h>
#include <avg/textentry.h>
#include <avg/targetimage.h>
#include <avg/font.h>
#include <avg/glyphbitmap.h>
#include <avg/positionedglyph.h>
#include <avg/brush.h>
#include <avg/pen.h>
#include <avg/pathbuilder.h>
#include <avg/shape.h>
#include <avg/color.h>
#include <avg/rect.h>
#include <avg/transform.h>

#include <ase/commandbuffer.h>
#include <ase/rendercommand.h>
#include <ase/drawcommand.h>
#include <ase/dummyplatform.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>
#include <ase/format.h>
#include <ase/vector.h>

#include <btl/runloop.h>

#include <pmr/new_delete_resource.h>

#include <utf8/utf8.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace
{
    std::string const fontPath = "data/fonts/OpenSans-Regular.ttf";
    constexpr size_t textVertexFloats = 9;
    constexpr float height = 12.0f;
    avg::Vector2f const offset(10.0f, 20.0f);

    struct Draw
    {
        ase::DrawCommand const* command;
        std::vector<float> vertices;
    };

    struct Fixture
    {
        btl::RunLoop loop;
        ase::Platform platform = ase::makeDummyPlatform(loop);
        ase::RenderContext context = platform.makeRenderContext();
        avg::Painter painter{ pmr::new_delete_resource(), context };
        avg::TargetImage target{ context, ase::Vector2i(200, 100),
            ase::FORMAT_SRGBA };
        avg::Font font = avg::Font(fontPath, 0);

        ase::CommandBuffer render(avg::Drawing const& drawing,
                float scalingFactor = 1.0f)
        {
            ase::CommandBuffer commands;
            avg::render(pmr::new_delete_resource(), commands, context,
                    target.getFramebuffer(), target.getSize(),
                    scalingFactor, painter, drawing);

            return commands;
        }

        avg::Drawing drawing(avg::TextEntry entry) const
        {
            return avg::Drawing(pmr::new_delete_resource(),
                    std::move(entry));
        }

        avg::TextEntry entry(std::string const& text,
                avg::Transform const& transform = avg::Transform()
                    .scale(height)
                    .translate(offset),
                std::optional<avg::Pen> pen = std::nullopt) const
        {
            return avg::TextEntry(font, transform, text,
                    avg::Brush(avg::Color(1.0f, 0.5f, 0.0f, 1.0f)), pen);
        }

        // The draws of commands, each with the vertex data uploaded for it
        std::vector<Draw> draws(ase::CommandBuffer const& commands) const
        {
            std::vector<Draw> result;
            for (auto const& command : commands)
            {
                auto const* draw = std::get_if<ase::DrawCommand>(&command);
                if (!draw)
                    continue;

                std::vector<float> vertices;
                for (auto const& candidate : commands)
                {
                    auto const* upload = std::get_if<ase::BufferUploadCommand>(
                            &candidate);
                    if (!upload)
                        continue;

                    auto const* vb = std::get_if<ase::VertexBuffer>(
                            &upload->target);
                    if (!vb || *vb != draw->getVertexBuffer())
                        continue;

                    float const* data = upload->data.mapRead<float>();
                    vertices.assign(data,
                            data + upload->data.getSize() / sizeof(float));
                }

                result.push_back(Draw{ draw, std::move(vertices) });
            }

            return result;
        }

        std::vector<Draw> textDraws(ase::CommandBuffer const& commands) const
        {
            std::vector<Draw> result;
            for (auto&& draw : draws(commands))
            {
                if (draw.command->getPipeline() == painter.getTextPipeline())
                    result.push_back(std::move(draw));
            }

            return result;
        }

        avg::GlyphBitmap bitmap(std::string const& text, int pixelSize,
                int bin = 0) const
        {
            auto glyphs = font.getGlyphs(utf8::asUtf8(text), 1.0f);
            return font.rasterizeGlyph(glyphs.at(0).glyphIndex, pixelSize,
                    bin);
        }
    };

    // One text vertex as x, y, u, v
    struct Corner
    {
        float x;
        float y;
        float u;
        float v;
    };

    std::vector<Corner> corners(std::vector<float> const& vertices)
    {
        std::vector<Corner> result;
        for (size_t i = 0; i + textVertexFloats <= vertices.size();
                i += textVertexFloats)
        {
            result.push_back(Corner{ vertices[i + 4], vertices[i + 5],
                    vertices[i + 7], vertices[i + 8] });
        }

        return result;
    }

    bool hasSubUpload(ase::CommandBuffer const& commands)
    {
        return std::any_of(commands.begin(), commands.end(),
                [](ase::RenderCommand const& c)
                {
                    return std::holds_alternative<ase::TextureSubUploadCommand>(
                            c);
                });
    }
} // anonymous namespace

TEST(rendering, unrotatedTextDrawsAtlasQuads)
{
    Fixture f;

    auto commands = f.render(f.drawing(f.entry("Hi")));

    auto text = f.textDraws(commands);
    ASSERT_EQ(1u, text.size());
    ASSERT_EQ(1u, text[0].command->getTextures().size());
    EXPECT_EQ(f.painter.getGlyphAtlas().getTexture(),
            text[0].command->getTextures()[0]);
    EXPECT_EQ(8u, corners(text[0].vertices).size());
    EXPECT_TRUE(hasSubUpload(commands));
    EXPECT_EQ(1u, f.draws(commands).size());
}

TEST(rendering, blendedDrawsKeepPaintOrder)
{
    Fixture f;
    avg::Brush const scrim(avg::Color(0.0f, 0.0f, 0.0f, 0.5f));
    auto rect = avg::Drawing::ShapeElement{
        avg::Shape(avg::PathBuilder(pmr::new_delete_resource())
                .start(0.0f, 0.0f)
                .lineTo(200.0f, 0.0f)
                .lineTo(200.0f, 100.0f)
                .lineTo(0.0f, 100.0f)
                .close()
                .build()),
        scrim,
        std::nullopt
    };

    auto commands = f.render(f.drawing(f.entry("Hi"))
            + avg::Drawing::Element(std::move(rect))
            + avg::Drawing::Element(f.entry("Ho")));

    auto draws = f.draws(commands);
    ASSERT_EQ(3u, draws.size());
    EXPECT_EQ(f.painter.getTextPipeline(), draws[0].command->getPipeline());
    EXPECT_EQ(f.painter.getPipeline(scrim), draws[1].command->getPipeline());
    EXPECT_EQ(f.painter.getTextPipeline(), draws[2].command->getPipeline());
    EXPECT_GT(draws[0].command->getZ(), draws[1].command->getZ());
    EXPECT_GT(draws[1].command->getZ(), draws[2].command->getZ());
}

TEST(rendering, quadMatchesTheGlyphBitmap)
{
    Fixture f;

    auto commands = f.render(f.drawing(f.entry("H")));
    auto text = f.textDraws(commands);
    ASSERT_EQ(1u, text.size());

    auto quad = corners(text[0].vertices);
    ASSERT_EQ(4u, quad.size());

    auto const b = f.bitmap("H", 12);
    auto const texSize = f.painter.getGlyphAtlas().getTextureSize();
    float const left = offset[0] + (float)b.bearingX;
    float const top = offset[1] + (float)b.bearingY;

    for (auto const& c : quad)
    {
        EXPECT_TRUE(c.x == left || c.x == left + (float)b.width) << c.x;
        EXPECT_TRUE(c.y == top || c.y == top - (float)b.height) << c.y;
    }

    auto const topCorner = std::min_element(quad.begin(), quad.end(),
            [](Corner const& a, Corner const& b) { return a.v < b.v; });
    auto const bottomCorner = std::max_element(quad.begin(), quad.end(),
            [](Corner const& a, Corner const& b) { return a.v < b.v; });

    // Atlas rows run from the top, so the top edge has the smaller v
    EXPECT_EQ(top, topCorner->y);
    EXPECT_EQ(top - (float)b.height, bottomCorner->y);
    EXPECT_FLOAT_EQ((float)b.height / (float)texSize[1],
            bottomCorner->v - topCorner->v);

    auto const [uMin, uMax] = std::minmax_element(quad.begin(), quad.end(),
            [](Corner const& a, Corner const& b) { return a.u < b.u; });
    EXPECT_FLOAT_EQ((float)b.width / (float)texSize[0], uMax->u - uMin->u);
    EXPECT_EQ(left, uMin->x);
}

TEST(rendering, quadsSnapToDevicePixelsAtScale)
{
    Fixture f;

    // 10.3 logical units at scale 2 is device x 20.6: pixel 20, bin 2
    auto transform = avg::Transform().scale(height)
        .translate(avg::Vector2f(10.3f, 20.2f));
    auto commands = f.render(f.drawing(f.entry("H", transform)), 2.0f);
    auto text = f.textDraws(commands);
    ASSERT_EQ(1u, text.size());

    auto quad = corners(text[0].vertices);
    ASSERT_EQ(4u, quad.size());

    auto const b = f.bitmap("H", 24, 2);
    float const left = (20.0f + (float)b.bearingX) / 2.0f;
    float const top = (40.0f + (float)b.bearingY) / 2.0f;

    for (auto const& c : quad)
    {
        EXPECT_TRUE(c.x == left || c.x == left + (float)b.width / 2.0f)
            << c.x;
        EXPECT_TRUE(c.y == top || c.y == top - (float)b.height / 2.0f)
            << c.y;
    }
}

TEST(rendering, clipRectCutsQuadsAndTheirTexels)
{
    Fixture f;

    auto const b = f.bitmap("H", 12);
    float const left = offset[0] + (float)b.bearingX;
    float const cut = left + std::floor((float)b.width / 2.0f);

    auto drawing = f.drawing(f.entry("H"))
        .clip(avg::Rect(avg::Vector2f(0.0f, 0.0f), avg::Vector2f(cut, 100.0f)));
    ASSERT_EQ(1u, drawing.getElements().size());
    ASSERT_TRUE(std::holds_alternative<avg::Drawing::ClipElement>(
                drawing.getElements()[0]));

    auto commands = f.render(drawing);
    auto text = f.textDraws(commands);
    ASSERT_EQ(1u, text.size());

    auto quad = corners(text[0].vertices);
    ASSERT_EQ(4u, quad.size());

    auto const texSize = f.painter.getGlyphAtlas().getTextureSize();
    auto const [uMin, uMax] = std::minmax_element(quad.begin(), quad.end(),
            [](Corner const& a, Corner const& b) { return a.u < b.u; });

    EXPECT_EQ(left, uMin->x);
    EXPECT_EQ(cut, uMax->x);
    EXPECT_FLOAT_EQ((cut - left) / (float)texSize[0], uMax->u - uMin->u);
}

TEST(rendering, rotatedTextDrawsOutlines)
{
    Fixture f;

    auto transform = avg::Transform().scale(height).rotate(0.3f)
        .translate(offset);
    auto commands = f.render(f.drawing(f.entry("Hi", transform)));

    EXPECT_TRUE(f.textDraws(commands).empty());
    EXPECT_FALSE(hasSubUpload(commands));
    ASSERT_EQ(1u, f.draws(commands).size());
    EXPECT_TRUE(f.draws(commands)[0].command->getTextures().empty());
}

TEST(rendering, strokedTextDrawsOutlines)
{
    Fixture f;

    auto commands = f.render(f.drawing(f.entry("Hi",
                    avg::Transform().scale(height).translate(offset),
                    avg::Pen(avg::Brush(avg::Color(0.0f, 0.0f, 1.0f, 1.0f)),
                        0.1f))));

    EXPECT_TRUE(f.textDraws(commands).empty());
    EXPECT_FALSE(hasSubUpload(commands));
    ASSERT_FALSE(f.draws(commands).empty());
    for (auto const& draw : f.draws(commands))
        EXPECT_TRUE(draw.command->getTextures().empty());
}

TEST(rendering, hugeTextDrawsOutlines)
{
    Fixture f;

    auto commands = f.render(f.drawing(f.entry("H",
                    avg::Transform().scale(200.0f)
                    .translate(avg::Vector2f(10.0f, -200.0f)))));

    EXPECT_TRUE(f.textDraws(commands).empty());
    EXPECT_EQ(1u, f.draws(commands).size());
}

TEST(rendering, blankTextDrawsNothing)
{
    Fixture f;

    auto commands = f.render(f.drawing(f.entry(" ")));

    EXPECT_TRUE(f.draws(commands).empty());
}
