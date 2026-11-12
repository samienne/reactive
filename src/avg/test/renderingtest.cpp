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
#include <ase/dummyrenderqueue.h>
#include <ase/dummyuniformset.h>
#include <ase/dummywindow.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>
#include <ase/renderqueue.h>
#include <ase/format.h>
#include <ase/texture.h>
#include <ase/window.h>
#include <ase/uniformbuffer.h>
#include <ase/uniformbufferimpl.h>
#include <ase/uniformset.h>
#include <ase/vertexspec.h>
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

    std::vector<ase::BufferUploadCommand const*> uniformUploads(
            ase::CommandBuffer const& commands)
    {
        std::vector<ase::BufferUploadCommand const*> result;
        for (auto const& command : commands)
        {
            auto const* upload = std::get_if<ase::BufferUploadCommand>(
                    &command);
            if (upload && std::holds_alternative<ase::UniformBuffer>(
                        upload->target))
            {
                result.push_back(upload);
            }
        }

        return result;
    }

    ase::UniformBufferImpl const* implOf(ase::UniformBuffer const& buffer)
    {
        return &buffer.getImpl<ase::UniformBufferImpl>();
    }

    ase::UniformBufferImpl const* uploadTarget(
            ase::BufferUploadCommand const& upload)
    {
        return implOf(std::get<ase::UniformBuffer>(upload.target));
    }

    ase::VertexSpec::Spec attribute(int location, int size, size_t offset)
    {
        return ase::VertexSpec::Spec{ location, size, ase::TypeFloat, false,
            offset };
    }

    // The commands of a frame other than the uploads feeding its draws
    std::vector<ase::RenderCommand const*> nonUploads(
            ase::CommandBuffer const& commands)
    {
        std::vector<ase::RenderCommand const*> result;
        for (auto const& command : commands)
        {
            if (!std::holds_alternative<ase::BufferUploadCommand>(command)
                    && !std::holds_alternative<ase::TextureUploadCommand>(
                        command)
                    && !std::holds_alternative<ase::TextureSubUploadCommand>(
                        command))
            {
                result.push_back(&command);
            }
        }

        return result;
    }

    bool hasBarrier(ase::CommandBuffer const& commands)
    {
        return std::any_of(commands.begin(), commands.end(),
                [](ase::RenderCommand const& c)
                {
                    return std::holds_alternative<ase::BarrierCommand>(c);
                });
    }

    // The last draw of a window frame: the one that presents
    ase::DrawCommand const* lastDraw(ase::CommandBuffer const& commands)
    {
        ase::DrawCommand const* result = nullptr;
        for (auto const& command : commands)
        {
            if (auto const* draw = std::get_if<ase::DrawCommand>(&command))
                result = draw;
        }

        return result;
    }

    // One window frame as bqui drives it, returning the frame's draw buffer
    ase::CommandBuffer const& windowFrame(Fixture& f, ase::Window& window,
            avg::Drawing const& drawing)
    {
        auto& queue = f.context.getMainRenderQueue()
            .getImpl<ase::DummyRenderQueue>();
        queue.recordSubmissions(true);
        size_t const before = queue.getSubmissions().size();

        f.painter.setSize(window.getScaledSize(), window.getScalingFactor());
        f.painter.clearWindow(window);
        f.painter.paintToWindow(window, drawing);
        f.painter.presentWindow(window);

        // The projection upload, then the draws
        EXPECT_EQ(before + 2, queue.getSubmissions().size());
        return queue.getSubmissions().back();
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

TEST(rendering, painterPipelinesKeepTheirDepthState)
{
    Fixture f;
    avg::Brush const solid(avg::Color(1.0f, 0.0f, 0.0f, 1.0f));
    avg::Brush const scrim(avg::Color(0.0f, 0.0f, 0.0f, 0.5f));

    ase::Pipeline const opaque = f.painter.getPipeline(solid);
    EXPECT_FALSE(opaque.isBlendEnabled());
    EXPECT_TRUE(opaque.isDepthTestEnabled());
    EXPECT_TRUE(opaque.isDepthWriteEnabled());

    for (ase::Pipeline const& blended : { f.painter.getPipeline(scrim),
            f.painter.getTextPipeline() })
    {
        EXPECT_TRUE(blended.isBlendEnabled());
        EXPECT_TRUE(blended.isDepthTestEnabled());
        EXPECT_FALSE(blended.isDepthWriteEnabled());
    }
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

TEST(rendering, aBackdropScopeDrawsLikeItsContent)
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

    auto drawing = avg::Drawing::over(
            f.drawing(f.entry("Hi")),
            avg::Drawing(pmr::new_delete_resource(), std::move(rect))
                + avg::Drawing::Element(f.entry("Ho")));
    ASSERT_EQ(2u, drawing.getElements().size());
    ASSERT_TRUE(std::holds_alternative<avg::Drawing::BackdropScope>(
                drawing.getElements()[1]));

    auto commands = f.render(drawing);
    auto draws = f.draws(commands);
    ASSERT_EQ(3u, draws.size());
    EXPECT_EQ(f.painter.getTextPipeline(), draws[0].command->getPipeline());
    EXPECT_EQ(f.painter.getPipeline(scrim), draws[1].command->getPipeline());
    EXPECT_EQ(f.painter.getTextPipeline(), draws[2].command->getPipeline());
    EXPECT_GT(draws[0].command->getZ(), draws[1].command->getZ());
    EXPECT_GT(draws[1].command->getZ(), draws[2].command->getZ());
}

TEST(rendering, aTranslatedScopePlacesTextLikeATranslatedDrawing)
{
    Fixture f;
    avg::Vector2f const shift(5.0f, 3.0f);

    auto plain = f.textDraws(f.render(
                f.drawing(f.entry("Hi")).translate(shift)));
    auto scoped = f.textDraws(f.render(
                f.drawing(f.entry("Hi")).scope().translate(shift)));

    ASSERT_EQ(1u, plain.size());
    ASSERT_EQ(1u, scoped.size());
    EXPECT_EQ(plain[0].vertices, scoped[0].vertices);
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

TEST(rendering, pipelinesResolveVerticesAgainstTheDeclaration)
{
    Fixture f;

    // Vertices are rgba, xyz (and uv for text): pos at location 0, color
    // at 1, uv at 2.
    auto const& solid = f.painter.getPipeline(
            avg::Brush(avg::Color(1.0f, 0.0f, 0.0f, 1.0f))).getVertexSpec();
    ASSERT_EQ(2u, solid.getSpecs().size());
    EXPECT_EQ(attribute(0, 3, 4 * sizeof(float)), solid.getSpecs()[0]);
    EXPECT_EQ(attribute(1, 4, 0), solid.getSpecs()[1]);
    EXPECT_EQ(7 * sizeof(float), solid.getStride());

    auto const& text = f.painter.getTextPipeline().getVertexSpec();
    ASSERT_EQ(3u, text.getSpecs().size());
    EXPECT_EQ(attribute(0, 3, 4 * sizeof(float)), text.getSpecs()[0]);
    EXPECT_EQ(attribute(1, 4, 0), text.getSpecs()[1]);
    EXPECT_EQ(attribute(2, 2, 7 * sizeof(float)), text.getSpecs()[2]);
    EXPECT_EQ(textVertexFloats * sizeof(float), text.getStride());
}

TEST(rendering, frameUploadsItsProjectionAheadOfItsDraws)
{
    Fixture f;
    auto& queue = f.context.getMainRenderQueue()
        .getImpl<ase::DummyRenderQueue>();
    queue.recordSubmissions(true);

    f.painter.setSize(ase::Vector2i(200, 100), 2.0f);
    f.painter.paintToImage(f.target, 1.0f, f.drawing(f.entry("Hi")));
    f.painter.flush();

    auto const& submitted = queue.getSubmissions();
    ASSERT_EQ(2u, submitted.size());

    // The first submission is the frame's one uniform upload: the
    // projection, column-major, mapping 200x100 device pixels at scale 2.
    ASSERT_EQ(1u, submitted[0].size());
    auto const* upload = std::get_if<ase::BufferUploadCommand>(
            &*submitted[0].begin());
    ASSERT_TRUE(upload);
    ASSERT_TRUE(std::holds_alternative<ase::UniformBuffer>(upload->target));
    ASSERT_EQ(16 * sizeof(float), upload->data.getSize());
    float const* m = upload->data.mapRead<float>();
    EXPECT_FLOAT_EQ(2.0f * 2.0f / 200.0f, m[0]);
    EXPECT_FLOAT_EQ(2.0f * 2.0f / 100.0f, m[5]);
    EXPECT_EQ(1.0f, m[10]);
    EXPECT_EQ(-1.0f, m[12]);
    EXPECT_EQ(-1.0f, m[13]);
    EXPECT_EQ(1.0f, m[15]);
    EXPECT_EQ(0.0f, m[1]);
    EXPECT_EQ(0.0f, m[4]);

    // The second holds the draws, every one binding the painter's set, with
    // no uniform upload among them to fence the batch.
    auto const& frame = submitted[1];
    EXPECT_TRUE(uniformUploads(frame).empty());
    auto draws = f.draws(frame);
    ASSERT_FALSE(draws.empty());
    for (auto const& draw : draws)
        EXPECT_EQ(f.painter.getUniformSet(), draw.command->getUniforms());

    auto const& bindings = f.painter.getUniformSet()
        .getImpl<ase::DummyUniformSet>().getBindings();
    ASSERT_EQ(1u, bindings.count(0));
    EXPECT_EQ(0u, bindings.at(0).offset);
    EXPECT_EQ(16 * sizeof(float), bindings.at(0).size);
    EXPECT_EQ(uploadTarget(*upload), implOf(bindings.at(0).buffer));

    // The next frame writes and binds another buffer of the ring.
    f.painter.paintToImage(f.target, 1.0f, f.drawing(f.entry("Ho")));
    f.painter.flush();

    ASSERT_EQ(4u, submitted.size());
    auto next = uniformUploads(submitted[2]);
    ASSERT_EQ(1u, next.size());
    EXPECT_NE(uploadTarget(*upload), uploadTarget(*next[0]));
    EXPECT_EQ(uploadTarget(*next[0]), implOf(bindings.at(0).buffer));
    EXPECT_TRUE(uniformUploads(submitted[3]).empty());
}

TEST(rendering, windowFrameDrawsOffscreenAndPresentsOneQuad)
{
    Fixture f;
    auto window = f.platform.makeWindow(f.context, ase::Vector2i(200, 100),
            false);
    auto const& frame = windowFrame(f, window, f.drawing(f.entry("Hi")));

    // Clear the target, draw into it, barrier, present onto the window.
    auto commands = nonUploads(frame);
    ASSERT_GE(commands.size(), 4u);

    auto const* clear = std::get_if<ase::ClearCommand>(commands.front());
    ASSERT_TRUE(clear);
    EXPECT_NE(window.getDefaultFramebuffer(), clear->target);
    EXPECT_TRUE(clear->color);
    EXPECT_TRUE(clear->depth);

    for (size_t i = 1; i + 2 < commands.size(); ++i)
    {
        auto const* draw = std::get_if<ase::DrawCommand>(commands[i]);
        ASSERT_TRUE(draw) << i;
        EXPECT_EQ(clear->target, draw->getFramebuffer());
    }

    EXPECT_TRUE(std::holds_alternative<ase::BarrierCommand>(
                *commands[commands.size() - 2]));

    auto const* present = std::get_if<ase::DrawCommand>(commands.back());
    ASSERT_TRUE(present);
    EXPECT_EQ(window.getDefaultFramebuffer(), present->getFramebuffer());
    EXPECT_FALSE(present->getPipeline().isBlendEnabled());
    EXPECT_FALSE(present->getPipeline().isDepthTestEnabled());
    EXPECT_FALSE(present->getPipeline().isDepthWriteEnabled());
    ASSERT_EQ(1u, present->getTextures().size());

    ase::Texture target = present->getTextures()[0];
    EXPECT_NE(f.painter.getGlyphAtlas().getTexture(), target);
    EXPECT_EQ(ase::Vector2i(200, 100), target.getSize());
    EXPECT_EQ(ase::FORMAT_SRGBA, target.getFormat());

    // The quad is the window's only command: no clear precedes it.
    for (auto const* command : commands)
    {
        if (auto const* c = std::get_if<ase::ClearCommand>(command))
            EXPECT_NE(window.getDefaultFramebuffer(), c->target);
    }
}

TEST(rendering, windowTargetIsRemadeOnlyWhenThePixelSizeChanges)
{
    Fixture f;
    auto window = f.platform.makeWindow(f.context, ase::Vector2i(200, 100),
            false);
    auto drawing = f.drawing(f.entry("Hi"));

    ase::Texture first = lastDraw(windowFrame(f, window, drawing))
        ->getTextures().at(0);
    ase::Texture second = lastDraw(windowFrame(f, window, drawing))
        ->getTextures().at(0);
    EXPECT_EQ(first, second);

    window.getImpl<ase::DummyWindow>().resize(ase::Vector2i(400, 100));
    ase::Texture resized = lastDraw(windowFrame(f, window, drawing))
        ->getTextures().at(0);
    EXPECT_NE(first, resized);
    EXPECT_EQ(ase::Vector2i(400, 100), resized.getSize());
}

TEST(rendering, imageFramesCarryNoPresent)
{
    Fixture f;
    auto& queue = f.context.getMainRenderQueue()
        .getImpl<ase::DummyRenderQueue>();
    queue.recordSubmissions(true);

    f.painter.clearImage(f.target);
    f.painter.paintToImage(f.target, 1.0f, f.drawing(f.entry("Hi")));
    f.painter.flush();

    ASSERT_EQ(2u, queue.getSubmissions().size());
    auto const& frame = queue.getSubmissions()[1];
    EXPECT_FALSE(hasBarrier(frame));
    auto draws = f.draws(frame);
    ASSERT_FALSE(draws.empty());
    for (auto const& draw : draws)
        EXPECT_EQ(f.target.getFramebuffer(), draw.command->getFramebuffer());
}

TEST(rendering, resizingDoesNotSubmit)
{
    Fixture f;
    auto& queue = f.context.getMainRenderQueue()
        .getImpl<ase::DummyRenderQueue>();
    queue.recordSubmissions(true);

    f.painter.setSize(ase::Vector2i(200, 100), 1.0f);
    f.painter.setSize(ase::Vector2i(400, 100), 1.0f);
    f.painter.flush();
    EXPECT_TRUE(queue.getSubmissions().empty());

    f.painter.clearImage(f.target);
    f.painter.flush();

    // A frame that only clears still carries its projection.
    ASSERT_EQ(2u, queue.getSubmissions().size());
    ASSERT_EQ(1u, uniformUploads(queue.getSubmissions()[0]).size());
    float const* m = uniformUploads(queue.getSubmissions()[0])[0]
        ->data.mapRead<float>();
    EXPECT_FLOAT_EQ(2.0f / 400.0f, m[0]);
}
