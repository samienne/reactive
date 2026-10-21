#include <glyphatlas.h>

#include <avg/font.h>
#include <avg/glyphbitmap.h>
#include <avg/positionedglyph.h>

#include <ase/commandbuffer.h>
#include <ase/rendercommand.h>
#include <ase/dummyplatform.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>
#include <ase/texture.h>

#include <btl/runloop.h>

#include <utf8/utf8.h>

#include <gtest/gtest.h>

#include <variant>

#include <cstdint>
#include <string>
#include <vector>

using avg::AtlasRect;
using avg::GlyphAtlas;
using avg::GlyphSlot;

namespace
{
    std::string const fontPath = "data/fonts/OpenSans-Regular.ttf";
    constexpr int pixelSize = 16;

    struct Fixture
    {
        btl::RunLoop loop;
        ase::Platform platform = ase::makeDummyPlatform(loop);
        ase::RenderContext context = platform.makeRenderContext();
        avg::Font font = avg::Font(fontPath, 0);

        std::vector<uint32_t> glyphs(char const* text) const
        {
            std::vector<uint32_t> result;
            for (auto const& glyph : font.getGlyphs(utf8::asUtf8(text), 1.0f))
                result.push_back(glyph.glyphIndex);

            return result;
        }
    };

    std::vector<uint32_t> const& letters(Fixture const& f)
    {
        static std::vector<uint32_t> const result =
            f.glyphs("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        return result;
    }

    // Fetches letters in order until one does not fit, returning the number
    // that did. The atlas is overflowed afterwards.
    size_t fill(GlyphAtlas& atlas, Fixture const& f)
    {
        auto const& glyphs = letters(f);
        for (size_t i = 0; i < glyphs.size(); ++i)
        {
            if (!atlas.get(f.font, glyphs[i], pixelSize, 0).has_value())
                return i;
        }

        ADD_FAILURE() << "atlas never overflowed";
        return glyphs.size();
    }
} // anonymous namespace

TEST(GlyphAtlas, cachesRasterisedGlyph)
{
    Fixture f;
    GlyphAtlas atlas(f.context);
    uint32_t a = f.glyphs("A").at(0);
    avg::GlyphBitmap bitmap = f.font.rasterizeGlyph(a, pixelSize, 0);

    atlas.beginFrame();
    auto slot = atlas.get(f.font, a, pixelSize, 0);
    ASSERT_TRUE(slot.has_value());
    EXPECT_EQ(bitmap.width, slot->rect.width);
    EXPECT_EQ(bitmap.height, slot->rect.height);
    EXPECT_GT(slot->rect.width, 0);
    EXPECT_GT(slot->rect.height, 0);
    EXPECT_EQ(bitmap.bearingX, slot->bearingX);
    EXPECT_EQ(bitmap.bearingY, slot->bearingY);
    EXPECT_LE(slot->rect.x + slot->rect.width, atlas.getTextureSize()[0]);
    EXPECT_LE(slot->rect.y + slot->rect.height, atlas.getTextureSize()[1]);

    auto again = atlas.get(f.font, a, pixelSize, 0);
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(slot->rect, again->rect);
    EXPECT_EQ(1u, atlas.getCount());

    ase::CommandBuffer commands;
    atlas.endFrame(commands);
    ASSERT_EQ(1u, commands.size());
    EXPECT_EQ(ase::Vector2i(512, 512), atlas.getTexture().getSize());

    // The upload covers the padding ring too, as zero, so an evicted
    // neighbour's texels never bleed into the quad's edge samples.
    auto const* upload = std::get_if<ase::TextureSubUploadCommand>(
            &*commands.begin());
    ASSERT_NE(nullptr, upload);
    int const width = bitmap.width + 2;
    int const height = bitmap.height + 2;
    EXPECT_EQ(ase::Vector2i(slot->rect.x - 1, slot->rect.y - 1),
            upload->offset);
    EXPECT_EQ(ase::Vector2i(width, height), upload->size);
    ASSERT_EQ(static_cast<size_t>(width * height), upload->data.getSize());
    auto const* texels = upload->data.mapRead<uint8_t>();
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            bool const border = x == 0 || y == 0
                || x == width - 1 || y == height - 1;
            uint8_t const expected = border
                ? 0
                : bitmap.coverage.at((y - 1) * bitmap.width + (x - 1));
            EXPECT_EQ(expected, texels[y * width + x])
                << "at " << x << "," << y;
        }
    }

    atlas.beginFrame();
    auto nextFrame = atlas.get(f.font, a, pixelSize, 0);
    ASSERT_TRUE(nextFrame.has_value());
    EXPECT_EQ(slot->rect, nextFrame->rect);

    ase::CommandBuffer none;
    atlas.endFrame(none);
    EXPECT_EQ(0u, none.size());
}

TEST(GlyphAtlas, blankGlyphHasEmptySlotAndNoUpload)
{
    Fixture f;
    GlyphAtlas atlas(f.context);
    uint32_t space = f.glyphs(" ").at(0);

    atlas.beginFrame();
    auto slot = atlas.get(f.font, space, pixelSize, 0);
    ASSERT_TRUE(slot.has_value());
    EXPECT_EQ(0, slot->rect.width);
    EXPECT_EQ(0, slot->rect.height);
    EXPECT_EQ(1u, atlas.getCount());

    ase::CommandBuffer commands;
    atlas.endFrame(commands);
    EXPECT_EQ(0u, commands.size());

    atlas.beginFrame();
    EXPECT_TRUE(atlas.get(f.font, space, pixelSize, 0).has_value());
    EXPECT_EQ(1u, atlas.getCount());
}

TEST(GlyphAtlas, overflowGrowsAtFrameEnd)
{
    Fixture f;
    GlyphAtlas atlas(f.context, 32, 2048);
    ase::Texture initial = atlas.getTexture();

    atlas.beginFrame();
    size_t fitted = fill(atlas, f);
    ASSERT_GT(fitted, 0u);
    ASSERT_LT(fitted + 1, letters(f).size());
    EXPECT_EQ(fitted, atlas.getCount());

    ase::CommandBuffer commands;
    atlas.endFrame(commands);
    EXPECT_EQ(fitted, commands.size());
    EXPECT_EQ(ase::Vector2i(64, 64), atlas.getTextureSize());
    EXPECT_EQ(ase::Vector2i(64, 64), atlas.getTexture().getSize());
    EXPECT_NE(initial, atlas.getTexture());
    EXPECT_EQ(0u, atlas.getCount());

    atlas.beginFrame();
    for (size_t i = 0; i <= fitted; ++i)
    {
        EXPECT_TRUE(atlas.get(f.font, letters(f)[i], pixelSize, 0)
                .has_value());
    }
    EXPECT_EQ(fitted + 1, atlas.getCount());
}

TEST(GlyphAtlas, overflowAtMaximumSizeClears)
{
    Fixture f;
    GlyphAtlas atlas(f.context, 32, 32);
    ase::Texture initial = atlas.getTexture();

    atlas.beginFrame();
    size_t fitted = fill(atlas, f);
    ASSERT_GT(fitted, 0u);

    ase::CommandBuffer commands;
    atlas.endFrame(commands);
    EXPECT_EQ(ase::Vector2i(32, 32), atlas.getTextureSize());
    EXPECT_EQ(initial, atlas.getTexture());
    EXPECT_EQ(0u, atlas.getCount());

    atlas.beginFrame();
    EXPECT_TRUE(atlas.get(f.font, letters(f)[0], pixelSize, 0).has_value());
    EXPECT_EQ(1u, atlas.getCount());
}

TEST(GlyphAtlas, framePinning)
{
    Fixture f;
    GlyphAtlas atlas(f.context, 32, 32);
    auto const& glyphs = letters(f);

    atlas.beginFrame();
    size_t fitted = fill(atlas, f);
    ASSERT_GT(fitted, 0u);

    std::vector<AtlasRect> rects;
    for (size_t i = 0; i < fitted; ++i)
    {
        auto slot = atlas.get(f.font, glyphs[i], pixelSize, 0);
        ASSERT_TRUE(slot.has_value());
        rects.push_back(slot->rect);
    }
    EXPECT_EQ(fitted, atlas.getCount());

    ase::CommandBuffer commands;
    atlas.endFrame(commands);
    EXPECT_EQ(0u, atlas.getCount());

    atlas.beginFrame();
    for (size_t i = 0; i < fitted; ++i)
    {
        auto slot = atlas.get(f.font, glyphs[i], pixelSize, 0);
        ASSERT_TRUE(slot.has_value());
        EXPECT_EQ(rects[i], slot->rect);
    }
    EXPECT_FALSE(atlas.get(f.font, glyphs[fitted], pixelSize, 0)
            .has_value());
    for (size_t i = 0; i < fitted; ++i)
    {
        auto slot = atlas.get(f.font, glyphs[i], pixelSize, 0);
        ASSERT_TRUE(slot.has_value());
        EXPECT_EQ(rects[i], slot->rect);
    }
    EXPECT_EQ(fitted, atlas.getCount());
    atlas.endFrame(commands);
    EXPECT_EQ(0u, atlas.getCount());

    atlas.beginFrame();
    for (size_t i = 0; i < fitted; ++i)
        ASSERT_TRUE(atlas.get(f.font, glyphs[i], pixelSize, 0).has_value());
    atlas.endFrame(commands);
    EXPECT_EQ(fitted, atlas.getCount());

    atlas.beginFrame();
    EXPECT_TRUE(atlas.get(f.font, glyphs[fitted], pixelSize, 0)
            .has_value());
    EXPECT_LE(atlas.getCount(), fitted);
    atlas.endFrame(commands);
    EXPECT_GT(atlas.getCount(), 0u);
}
