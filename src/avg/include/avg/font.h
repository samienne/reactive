#pragma once

#include "avgvisibility.h"

#include <ase/vector.h>

#include <btl/hash.h>

#include <pmr/memory_resource.h>

#include <utf8/utf8.h>

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace avg
{
    class FontImpl;
    struct TextExtents;
    struct PositionedGlyph;
    struct GlyphBitmap;
    class Path;
    class FontManager;

    /**
     * @brief Number of horizontal subpixel offsets Font::rasterizeGlyph
     * distinguishes.
     */
    constexpr int subpixelBins = 4;

    class AVG_EXPORT Font
    {
    public:
        enum Hinting
        {
            HINT_FULL,
            HINT_NONE
        };

        Font(std::string const& file, unsigned int face);
        Font(Font const&) = default;
        Font(Font&&) = default;

        ~Font();

        Font& operator=(Font const&) = default;
        Font& operator=(Font&&) = default;

        TextExtents getTextExtents(utf8::Utf8View text, float size) const;
        Path textToPath(pmr::memory_resource* memory, utf8::Utf8View text,
                float height, ase::Vector2f pos,
                Hinting hinting = HINT_NONE) const;
        size_t getCharacterIndex(utf8::Utf8View, float fontHeight,
                ase::Vector2f pos) const;

        /**
         * @brief The glyphs that drawing the text places, with their pen
         * origins.
         *
         * Lays out the text exactly as textToPath does, scaled by height
         * and starting from the origin, but returns one entry per glyph
         * instead of the outlines. A newline moves the pen to the start of
         * the next line; it produces no glyph of its own.
         */
        std::vector<PositionedGlyph> getGlyphs(utf8::Utf8View text,
                float height) const;

        /**
         * @brief Renders one glyph to an antialiased coverage bitmap.
         *
         * glyphIndex is a glyph from getGlyphs. pixelSize is the pixels one
         * unit of height covers, so a glyph that getGlyphs placed with
         * height h and drawn at scale s is rasterised with pixelSize h * s.
         * subpixelBin, 0 to subpixelBins - 1, shifts the outline right by
         * 1 / subpixelBins of a pixel per bin before sampling. Throws
         * std::invalid_argument for a size below one or a bin out of range.
         */
        GlyphBitmap rasterizeGlyph(uint32_t glyphIndex, int pixelSize,
                int subpixelBin) const;

        float getDescender(float height) const;
        float getAscender(float height) const;
        float getLinegap(float height) const;
        float getLinespace(float height) const;

        bool operator==(Font const& rhs) const;
        bool operator!=(Font const& rhs) const;
        bool operator<(Font const& rhs) const;
        bool operator>(Font const& rhs) const;

        bool isEmpty() const;

        /**
         * @brief An address that identifies the font behind this handle.
         *
         * Fonts that compare equal share it, and it stays unique while a
         * Font of that font exists. An empty font yields nullptr.
         */
        void const* getIdentity() const;

        AVG_EXPORT friend std::ostream& operator<<(std::ostream& stream,
                Font const& f);

        template <class THash>
        friend void hash_append(THash& h, Font const& font) noexcept
        {
            using btl::hash_append;
            hash_append(h, font.deferred_.get());
        }
    private:
        friend class FontManager;
        friend class FontManagerDeferred;
        inline FontImpl* d() { return deferred_.get(); }
        inline FontImpl const* d() const { return deferred_.get(); }
        std::shared_ptr<FontImpl> deferred_;
    };
}

