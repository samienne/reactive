#include "gtest/gtest.h"

#include <avg/font.h>
#include <avg/fontmanager.h>
#include <avg/glyphbitmap.h>
#include <avg/path.h>
#include <avg/pathbuilder.h>
#include <avg/positionedglyph.h>
#include <avg/rect.h>
#include <avg/textextents.h>
#include <ase/vector.h>

#include <pmr/new_delete_resource.h>

#include <utf8/utf8.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <string>

const std::string fontPath = "data/fonts/OpenSans-Regular.ttf";

TEST(Font, ToPath)
{
    avg::Font font(fontPath, 0);

    avg::Path path = font.textToPath(pmr::new_delete_resource(),
            utf8::asUtf8("jepulis"), 18.0f / 800.0f,
            ase::Vector2f(-0.5f, -0.5f));

    //std::cout << path << std::endl;

    EXPECT_EQ(false, path.isEmpty());
}

TEST(Font, compare)
{
    avg::Font font(fontPath, 0);

    avg::Font font2(fontPath, 0);

    EXPECT_EQ(font, font2);
}

TEST(Font, missingFileThrows)
{
    std::string const missing = "data/fonts/does-not-exist.ttf";

    try
    {
        avg::Font font(missing, 0);
        FAIL() << "expected std::runtime_error";
    }
    catch (std::runtime_error const& e)
    {
        EXPECT_NE(std::string::npos, std::string(e.what()).find(missing));
    }
}

TEST(Font, reloadAfterRelease)
{
    {
        avg::Font font(fontPath, 0);
        EXPECT_FALSE(font.isEmpty());
    }

    avg::Font font(fontPath, 0);

    EXPECT_FALSE(font.isEmpty());
    EXPECT_GT(font.getAscender(1.0f), 0.0f);
    EXPECT_FALSE(font.textToPath(pmr::new_delete_resource(),
            utf8::asUtf8("jepulis"), 1.0f, ase::Vector2f(0.0f, 0.0f))
            .isEmpty());
}

namespace
{
    uint32_t glyphOf(avg::Font const& font, std::string const& character)
    {
        auto glyphs = font.getGlyphs(utf8::asUtf8(character), 1.0f);
        EXPECT_EQ(1u, glyphs.size());
        return glyphs.at(0).glyphIndex;
    }
}

TEST(Font, getGlyphsFollowsTextLayout)
{
    avg::Font font(fontPath, 0);
    float const height = 18.0f;

    auto glyphs = font.getGlyphs(utf8::asUtf8("Ab\nc"), height);

    ASSERT_EQ(3u, glyphs.size());

    auto a = font.getTextExtents(utf8::asUtf8("A"), height);
    auto ab = font.getTextExtents(utf8::asUtf8("Ab"), height);

    EXPECT_FLOAT_EQ(0.0f, glyphs[0].position[0]);
    EXPECT_FLOAT_EQ(0.0f, glyphs[0].position[1]);
    EXPECT_FLOAT_EQ(a.advance[0], glyphs[1].position[0]);
    EXPECT_FLOAT_EQ(0.0f, glyphs[1].position[1]);
    EXPECT_FLOAT_EQ(0.0f, glyphs[2].position[0]);
    EXPECT_FLOAT_EQ(-font.getLinegap(height), glyphs[2].position[1]);
    EXPECT_GT(ab.advance[0], a.advance[0]);

    EXPECT_NE(glyphs[0].glyphIndex, glyphs[1].glyphIndex);
    EXPECT_NE(glyphs[1].glyphIndex, glyphs[2].glyphIndex);
    EXPECT_NE(0u, glyphs[0].glyphIndex);
}

TEST(Font, rasterizeGlyphMatchesOutlineHeight)
{
    avg::Font font(fontPath, 0);
    uint32_t glyph = glyphOf(font, "A");

    avg::Rect bb = font.textToPath(pmr::new_delete_resource(),
            utf8::asUtf8("A"), 1.0f, ase::Vector2f(0.0f, 0.0f))
        .getControlBb();

    for (int pixelSize : { 12, 24, 48 })
    {
        avg::GlyphBitmap bitmap = font.rasterizeGlyph(glyph, pixelSize, 0);

        EXPECT_EQ((size_t)bitmap.width * bitmap.height,
                bitmap.coverage.size());
        EXPECT_NEAR(bb.getHeight() * pixelSize, bitmap.height, 1.0)
            << "pixelSize " << pixelSize;
        EXPECT_NEAR(bb.getWidth() * pixelSize, bitmap.width, 1.0)
            << "pixelSize " << pixelSize;
        EXPECT_NEAR(bb.getTop() * pixelSize, bitmap.bearingY, 1.0)
            << "pixelSize " << pixelSize;
        EXPECT_NEAR(bb.getLeft() * pixelSize, bitmap.bearingX, 1.0)
            << "pixelSize " << pixelSize;
    }
}

TEST(Font, rasterizeGlyphCoverage)
{
    avg::Font font(fontPath, 0);

    avg::GlyphBitmap bitmap = font.rasterizeGlyph(glyphOf(font, "A"), 48, 0);

    ASSERT_FALSE(bitmap.coverage.empty());
    EXPECT_EQ(255, *std::max_element(bitmap.coverage.begin(),
                bitmap.coverage.end()));
    EXPECT_EQ(0, *std::min_element(bitmap.coverage.begin(),
                bitmap.coverage.end()));
}

TEST(Font, rasterizeGlyphSubpixelBins)
{
    avg::Font font(fontPath, 0);
    uint32_t glyph = glyphOf(font, "A");

    std::vector<avg::GlyphBitmap> bins;
    for (int bin = 0; bin < 4; ++bin)
        bins.push_back(font.rasterizeGlyph(glyph, 24, bin));

    bool anyDiffer = false;
    for (size_t i = 0; i < bins.size(); ++i)
    {
        for (size_t j = i + 1; j < bins.size(); ++j)
        {
            EXPECT_LE(std::abs(bins[i].width - bins[j].width), 1);
            EXPECT_EQ(bins[i].height, bins[j].height);
            anyDiffer = anyDiffer || bins[i].coverage != bins[j].coverage;
        }
    }

    EXPECT_TRUE(anyDiffer);
    EXPECT_THROW(font.rasterizeGlyph(glyph, 24, 4), std::invalid_argument);
    EXPECT_THROW(font.rasterizeGlyph(glyph, 0, 0), std::invalid_argument);
}

TEST(Font, rasterizeGlyphKeepsTextExtents)
{
    avg::Font font(fontPath, 0);
    auto text = utf8::asUtf8("Ab\ncd");

    auto before = font.getTextExtents(text, 18.0f);
    auto path = font.textToPath(pmr::new_delete_resource(), text, 18.0f,
            ase::Vector2f(0.0f, 0.0f));

    font.rasterizeGlyph(glyphOf(font, "A"), 12, 1);
    font.rasterizeGlyph(glyphOf(font, "c"), 48, 3);

    // The metrics measured above are cached, so push them out with more
    // glyphs than the cache keeps: the measurement below then reads the
    // face again, at whatever size rasterizeGlyph left active.
    std::string flood;
    for (uint32_t code = 0x21; code < 0x250; ++code)
        flood += utf8::encode(code);
    for (uint32_t code = 0x391; code < 0x450; ++code)
        flood += utf8::encode(code);
    font.getTextExtents(utf8::asUtf8(flood), 18.0f);

    auto after = font.getTextExtents(text, 18.0f);
    auto pathAfter = font.textToPath(pmr::new_delete_resource(), text,
            18.0f, ase::Vector2f(0.0f, 0.0f));

    EXPECT_EQ(before.bearing, after.bearing);
    EXPECT_EQ(before.size, after.size);
    EXPECT_EQ(before.advance, after.advance);
    EXPECT_EQ(path.getControlBb(), pathAfter.getControlBb());
}
