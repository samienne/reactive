#pragma once

#include <avg/avgvisibility.h>

#include <btl/fnv1a.h>
#include <btl/hash.h>
#include <btl/uhash.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <unordered_map>
#include <vector>

namespace avg
{
    /**
     * @brief Identity of one rasterised glyph in a glyph atlas.
     *
     * Two keys are equal when every field is equal.
     */
    struct GlyphKey
    {
        void const* font = nullptr;
        uint32_t glyphIndex = 0;
        uint16_t pixelSize = 0;
        uint8_t subpixelBin = 0;

        friend bool operator==(GlyphKey const& a, GlyphKey const& b) noexcept
        {
            return a.font == b.font
                && a.glyphIndex == b.glyphIndex
                && a.pixelSize == b.pixelSize
                && a.subpixelBin == b.subpixelBin;
        }

        friend bool operator!=(GlyphKey const& a, GlyphKey const& b) noexcept
        {
            return !(a == b);
        }

        template <class THash>
        friend void hash_append(THash& h, GlyphKey const& key) noexcept
        {
            using btl::hash_append;
            hash_append(h, key.font);
            hash_append(h, key.glyphIndex);
            hash_append(h, key.pixelSize);
            hash_append(h, key.subpixelBin);
        }
    };

    using GlyphKeyHash = btl::uhash<btl::fnv1a>;

    /**
     * @brief A pixel rectangle inside an atlas; y grows downwards.
     */
    struct AtlasRect
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        friend bool operator==(AtlasRect const& a, AtlasRect const& b) noexcept
        {
            return a.x == b.x
                && a.y == b.y
                && a.width == b.width
                && a.height == b.height;
        }

        friend bool operator!=(AtlasRect const& a, AtlasRect const& b) noexcept
        {
            return !(a == b);
        }

        friend std::ostream& operator<<(
                std::ostream& stream,
                AtlasRect const& r)
        {
            return stream << "AtlasRect{" << r.x << "," << r.y << " "
                << r.width << "x" << r.height << "}";
        }
    };

    /**
     * @brief Bookkeeping for a glyph atlas texture: where each glyph lives,
     * and which one to drop when the texture is full.
     *
     * The allocator knows the texture's size but never its pixels: the caller
     * inserts a glyph, writes it into the returned rectangle, and finds it
     * again by key. Every allocation carries one pixel of padding on each side
     * that the caller leaves untouched, so bilinear sampling at a glyph's edge
     * never reads a neighbour; the returned rectangle is the area inside that
     * padding and has exactly the requested size.
     *
     * Frames bound eviction. Call beginFrame() once per frame; every glyph
     * inserted or found since the last call is pinned and is never evicted
     * before the next one. When an insert does not fit, the least recently
     * used unpinned glyphs are evicted until it does; it fails only when
     * nothing unpinned is left, and the caller then draws that glyph some
     * other way for the rest of the frame. Growing the atlas is the caller's
     * job: construct a larger allocator and start over.
     */
    class AVG_EXPORT AtlasAllocator
    {
    public:
        AtlasAllocator(int width, int height);

        /**
         * @brief Starts a frame: unpins every glyph pinned by the previous
         * one.
         */
        void beginFrame();

        /**
         * @brief Looks up a glyph, pinning it for this frame.
         *
         * Returns the rectangle insert() gave the key, or nullopt when the
         * glyph is not (or no longer) in the atlas.
         */
        std::optional<AtlasRect> find(GlyphKey const& key);

        /**
         * @brief Allocates room for a glyph of the given size, pinning it for
         * this frame.
         *
         * Returns the rectangle the caller writes the glyph into, or nullopt
         * when it does not fit even after evicting every unpinned glyph. Both
         * dimensions must be positive, and the key must not be in the atlas
         * already.
         */
        std::optional<AtlasRect> insert(
                GlyphKey const& key,
                int width,
                int height);

        /**
         * @brief Forgets every glyph; the frame count carries on.
         */
        void clear();

        int getWidth() const;
        int getHeight() const;
        size_t getCount() const;

    private:
        struct Range
        {
            int x;
            int width;
        };

        struct Shelf
        {
            int y;
            int height;
            std::vector<Range> free;
        };

        struct Entry
        {
            GlyphKey key;
            AtlasRect rect;
            int allocatedWidth;
            int shelf;
            uint64_t frame;
            int prev;
            int next;
        };

        struct Placement
        {
            int shelf;
            int x;
        };

        std::optional<Placement> allocate(int allocatedWidth, int shelfHeight);
        bool evictLeastRecentlyUsed();
        void returnRange(int shelf, int x, int width);
        void releaseTrailingShelves();
        bool isShelfFree(Shelf const& shelf) const;

        void touch(int index);
        void linkFront(int index);
        void unlink(int index);

        int width_;
        int height_;
        uint64_t frame_ = 0;
        int stripY_ = 0;
        int head_ = -1;
        int tail_ = -1;
        std::vector<Shelf> shelves_;
        std::vector<Entry> entries_;
        std::vector<int> freeEntries_;
        std::unordered_map<GlyphKey, int, GlyphKeyHash> map_;
    };
} // namespace avg

