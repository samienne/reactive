#include <atlasallocator.h>

#include <gtest/gtest.h>

using avg::AtlasAllocator;
using avg::AtlasRect;
using avg::GlyphKey;

namespace
{
    int const fontTag = 0;

    GlyphKey key(uint32_t glyphIndex)
    {
        GlyphKey k;
        k.font = &fontTag;
        k.glyphIndex = glyphIndex;
        k.pixelSize = 16;
        return k;
    }

    // A 16 px glyph occupies an 18 px shelf, so a 64x64 atlas holds three
    // shelves of three glyphs each.
    constexpr int glyphSize = 16;
    constexpr int capacity = 9;
} // anonymous namespace

TEST(AtlasAllocator, insertThenFind)
{
    AtlasAllocator atlas(64, 64);

    auto inserted = atlas.insert(key(1), 10, 12);
    ASSERT_TRUE(inserted.has_value());
    EXPECT_EQ((AtlasRect{ 1, 1, 10, 12 }), *inserted);

    auto found = atlas.find(key(1));
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*inserted, *found);

    EXPECT_FALSE(atlas.find(key(2)).has_value());
    EXPECT_EQ(1u, atlas.getCount());
    EXPECT_EQ(64, atlas.getWidth());
    EXPECT_EQ(64, atlas.getHeight());
}

TEST(AtlasAllocator, keyEqualityAndHash)
{
    GlyphKey a = key(1);
    GlyphKey b = key(1);
    GlyphKey c = key(1);
    c.subpixelBin = 2;

    avg::GlyphKeyHash hash;

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_EQ(hash(a), hash(b));
    EXPECT_NE(hash(a), hash(c));
}

TEST(AtlasAllocator, evictedRangeIsReused)
{
    AtlasAllocator atlas(64, 64);

    auto a = atlas.insert(key(0), glyphSize, glyphSize);
    auto b = atlas.insert(key(1), glyphSize, glyphSize);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    atlas.beginFrame();
    for (uint32_t i = 2; i < capacity; ++i)
        ASSERT_TRUE(atlas.insert(key(i), glyphSize, glyphSize).has_value());

    EXPECT_EQ(static_cast<size_t>(capacity), atlas.getCount());

    auto c = atlas.insert(key(capacity), glyphSize, glyphSize);
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(*a, *c);
    EXPECT_FALSE(atlas.find(key(0)).has_value());
    EXPECT_EQ(*b, *atlas.find(key(1)));
    EXPECT_EQ(static_cast<size_t>(capacity), atlas.getCount());
}

TEST(AtlasAllocator, bestFitPrefersSmallestGap)
{
    AtlasAllocator atlas(64, 18);

    // One shelf: A [0,18) B [18,28) C [28,38) D [38,56), 8 px left over.
    ASSERT_TRUE(atlas.insert(key(0), 16, glyphSize).has_value());
    ASSERT_TRUE(atlas.insert(key(1), 8, glyphSize).has_value());
    ASSERT_TRUE(atlas.insert(key(2), 8, glyphSize).has_value());
    ASSERT_TRUE(atlas.insert(key(3), 16, glyphSize).has_value());

    atlas.beginFrame();
    ASSERT_TRUE(atlas.find(key(1)).has_value());
    ASSERT_TRUE(atlas.find(key(3)).has_value());

    // Evicts A and C, then gives up with B and D pinned.
    EXPECT_FALSE(atlas.insert(key(4), 40, glyphSize).has_value());
    EXPECT_EQ(2u, atlas.getCount());
    EXPECT_FALSE(atlas.find(key(0)).has_value());
    EXPECT_FALSE(atlas.find(key(2)).has_value());

    // Gaps of 18, 10 and 8 px: the 10 px one (C's) fits best.
    auto f = atlas.insert(key(5), 8, glyphSize);
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ((AtlasRect{ 29, 1, 8, glyphSize }), *f);
}

TEST(AtlasAllocator, lruOrder)
{
    AtlasAllocator atlas(64, 64);

    for (uint32_t i = 0; i < capacity; ++i)
        ASSERT_TRUE(atlas.insert(key(i), glyphSize, glyphSize).has_value());

    atlas.beginFrame();
    ASSERT_TRUE(atlas.find(key(0)).has_value());

    ASSERT_TRUE(atlas.insert(key(capacity), glyphSize, glyphSize).has_value());

    EXPECT_TRUE(atlas.find(key(0)).has_value());
    EXPECT_FALSE(atlas.find(key(1)).has_value());
    EXPECT_TRUE(atlas.find(key(2)).has_value());
    EXPECT_EQ(static_cast<size_t>(capacity), atlas.getCount());
}

TEST(AtlasAllocator, framePinning)
{
    AtlasAllocator atlas(64, 18);

    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_TRUE(atlas.insert(key(i), glyphSize, glyphSize).has_value());

    EXPECT_FALSE(atlas.insert(key(3), glyphSize, glyphSize).has_value());
    EXPECT_EQ(3u, atlas.getCount());
    for (uint32_t i = 0; i < 3; ++i)
        EXPECT_TRUE(atlas.find(key(i)).has_value());

    atlas.beginFrame();

    EXPECT_TRUE(atlas.insert(key(3), glyphSize, glyphSize).has_value());
    EXPECT_EQ(3u, atlas.getCount());
    EXPECT_FALSE(atlas.find(key(0)).has_value());
    EXPECT_TRUE(atlas.find(key(1)).has_value());
    EXPECT_TRUE(atlas.find(key(2)).has_value());
}

TEST(AtlasAllocator, shelfReleaseAndRecut)
{
    AtlasAllocator atlas(64, 64);

    for (uint32_t i = 0; i < capacity; ++i)
        ASSERT_TRUE(atlas.insert(key(i), glyphSize, glyphSize).has_value());

    atlas.beginFrame();

    // A 40 px glyph needs a 42 px shelf: every 18 px shelf has to go.
    auto a = atlas.insert(key(100), 16, 40);
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ((AtlasRect{ 1, 1, 16, 40 }), *a);
    EXPECT_EQ(1u, atlas.getCount());

    auto b = atlas.insert(key(101), 16, 40);
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ((AtlasRect{ 19, 1, 16, 40 }), *b);

    auto c = atlas.insert(key(102), 16, 40);
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ((AtlasRect{ 37, 1, 16, 40 }), *c);

    EXPECT_FALSE(atlas.insert(key(103), 16, 40).has_value());
    EXPECT_EQ(3u, atlas.getCount());
}

TEST(AtlasAllocator, heightBucketing)
{
    AtlasAllocator atlas(64, 64);

    auto a = atlas.insert(key(0), 8, 13);
    auto b = atlas.insert(key(1), 8, 16);
    auto c = atlas.insert(key(2), 8, 17);
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    ASSERT_TRUE(c.has_value());

    EXPECT_EQ((AtlasRect{ 1, 1, 8, 13 }), *a);
    EXPECT_EQ((AtlasRect{ 11, 1, 8, 16 }), *b);
    EXPECT_EQ((AtlasRect{ 1, 19, 8, 17 }), *c);
}

TEST(AtlasAllocator, tooLargeNeverFits)
{
    AtlasAllocator atlas(64, 64);

    EXPECT_FALSE(atlas.insert(key(0), 63, 10).has_value());
    EXPECT_FALSE(atlas.insert(key(1), 10, 63).has_value());
    EXPECT_TRUE(atlas.insert(key(2), 62, 58).has_value());
}

TEST(AtlasAllocator, clear)
{
    AtlasAllocator atlas(64, 64);

    ASSERT_TRUE(atlas.insert(key(0), glyphSize, glyphSize).has_value());
    ASSERT_TRUE(atlas.insert(key(1), glyphSize, glyphSize).has_value());

    atlas.clear();

    EXPECT_EQ(0u, atlas.getCount());
    EXPECT_FALSE(atlas.find(key(0)).has_value());
    EXPECT_FALSE(atlas.find(key(1)).has_value());

    auto a = atlas.insert(key(0), glyphSize, glyphSize);
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ((AtlasRect{ 1, 1, glyphSize, glyphSize }), *a);
}

