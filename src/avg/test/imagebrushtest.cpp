#include <avg/brush.h>
#include <avg/drawing.h>
#include <avg/image.h>
#include <avg/painter.h>
#include <avg/pathbuilder.h>
#include <avg/rendering.h>
#include <avg/shape.h>

#include <ase/commandbuffer.h>
#include <ase/dummyplatform.h>
#include <ase/framebuffer.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>
#include <ase/texture.h>

#include <btl/runloop.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <vector>

using namespace avg;

namespace
{
    pmr::memory_resource* memory()
    {
        return pmr::new_delete_resource();
    }

    Drawing fillRect(float x, float y, float width, float height,
            Brush const& brush)
    {
        return Shape(PathBuilder(memory())
                .start(x, y)
                .lineTo(x + width, y)
                .lineTo(x + width, y + height)
                .lineTo(x, y + height)
                .close()
                .build())
            .fill(brush);
    }

    struct Rendered
    {
        ase::CommandBuffer commands;

        std::vector<ase::DrawCommand const*> draws() const
        {
            std::vector<ase::DrawCommand const*> result;
            for (auto const& command : commands)
            {
                if (auto draw = std::get_if<ase::DrawCommand>(&command))
                    result.push_back(draw);
            }
            return result;
        }

        // Each vertex is rgba, xyz, uv.
        std::vector<float> vertices(ase::DrawCommand const& draw) const
        {
            for (auto const& command : commands)
            {
                auto upload = std::get_if<ase::BufferUploadCommand>(&command);
                if (!upload)
                    continue;

                auto vb = std::get_if<ase::VertexBuffer>(&upload->target);
                if (!vb || *vb != draw.getVertexBuffer())
                    continue;

                auto data = upload->data.mapRead<float>();
                return std::vector<float>(data,
                        data + upload->data.getSize() / sizeof(float));
            }
            return {};
        }
    };

    class ImageBrush : public ::testing::Test
    {
    protected:
        ImageBrush() :
            platform_(ase::makeDummyPlatform(loop_)),
            context_(platform_.makeRenderContext()),
            painter_(memory(), context_)
        {
        }

        ase::Texture makeTexture()
        {
            return context_.makeTexture(ase::Vector2i(4, 2),
                    ase::FORMAT_SRGBA);
        }

        Rendered render(Drawing const& drawing)
        {
            Rendered result;
            ase::Framebuffer framebuffer = context_.makeFramebuffer();
            avg::render(memory(), result.commands, context_, framebuffer,
                    ase::Vector2i(100, 100), 1.0f, painter_, drawing);
            return result;
        }

        btl::RunLoop loop_;
        ase::Platform platform_;
        ase::RenderContext context_;
        Painter painter_;
    };
} // anonymous namespace

TEST_F(ImageBrush, comparesByTextureAndRowOrder)
{
    auto texture = makeTexture();
    Image image(texture);

    EXPECT_EQ(ase::Vector2i(4, 2), image.getSize());
    EXPECT_EQ(Brush(image), Brush(Image(texture)));
    EXPECT_NE(Brush(image), Brush(Image(makeTexture())));
    EXPECT_NE(Brush(image),
            Brush(Image(texture, Image::RowOrder::BottomFirst)));
    EXPECT_NE(Brush(image), Brush());
    EXPECT_NE(Brush(image), Brush(image, Color(1, 1, 1, 0.5f)));
}

TEST_F(ImageBrush, stretchesTheImageOverTheShapeBounds)
{
    auto texture = makeTexture();
    Color const tint(1.0f, 1.0f, 1.0f, 0.5f);
    auto rendered = render(fillRect(10.0f, 20.0f, 40.0f, 30.0f,
                Brush(Image(texture), tint)));

    auto draws = rendered.draws();
    ASSERT_EQ(1u, draws.size());
    EXPECT_EQ(std::vector<ase::Texture>{ texture },
            draws[0]->getTextures());

    auto vertices = rendered.vertices(*draws[0]);
    ASSERT_FALSE(vertices.empty());
    ASSERT_EQ(0u, vertices.size() % 9);
    for (size_t i = 0; i < vertices.size(); i += 9)
    {
        float const* v = &vertices[i];
        EXPECT_FLOAT_EQ(0.5f, v[0]);
        EXPECT_FLOAT_EQ(0.5f, v[3]);
        EXPECT_NEAR((v[4] - 10.0f) / 40.0f, v[7], 1e-4f);
        EXPECT_NEAR(1.0f - (v[5] - 20.0f) / 30.0f, v[8], 1e-4f);
    }
}

TEST_F(ImageBrush, bottomFirstRowOrderFlipsV)
{
    Image image(makeTexture(), Image::RowOrder::BottomFirst);
    auto rendered = render(fillRect(0.0f, 0.0f, 10.0f, 10.0f,
                Brush(image)));

    auto draws = rendered.draws();
    ASSERT_EQ(1u, draws.size());

    auto vertices = rendered.vertices(*draws[0]);
    ASSERT_FALSE(vertices.empty());
    for (size_t i = 0; i < vertices.size(); i += 9)
        EXPECT_NEAR(vertices[i + 5] / 10.0f, vertices[i + 8], 1e-4f);
}

TEST_F(ImageBrush, batchesSplitOnTexture)
{
    auto first = makeTexture();
    auto second = makeTexture();

    Drawing drawing(memory());
    drawing += fillRect(0.0f, 0.0f, 10.0f, 10.0f, Brush(Image(first)));
    drawing += fillRect(20.0f, 0.0f, 10.0f, 10.0f, Brush(Image(second)));
    drawing += fillRect(40.0f, 0.0f, 10.0f, 10.0f, Brush(Image(first)));
    drawing += fillRect(60.0f, 0.0f, 10.0f, 10.0f, Brush(Color()));

    auto rendered = render(drawing);
    auto draws = rendered.draws();
    ASSERT_EQ(3u, draws.size());

    int firstCount = 0;
    int secondCount = 0;
    int untextured = 0;
    for (auto draw : draws)
    {
        auto const& textures = draw->getTextures();
        if (textures.empty())
            ++untextured;
        else if (textures == std::vector<ase::Texture>{ first })
            ++firstCount;
        else if (textures == std::vector<ase::Texture>{ second })
            ++secondCount;
    }

    EXPECT_EQ(1, firstCount);
    EXPECT_EQ(1, secondCount);
    EXPECT_EQ(1, untextured);
}
