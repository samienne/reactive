#pragma once

#include "vector.h"
#include "avgvisibility.h"

#include <pmr/memory_resource.h>

#include <utf8/utf8.h>

#include <cstdint>
#include <mutex>
#include <memory>
#include <vector>

namespace avg
{
    class FontImpl;
    class Font;
    class Path;
    struct TextExtents;
    struct PositionedGlyph;
    struct GlyphBitmap;
    class FontManagerDeferred;

    class AVG_EXPORT FontManager
    {
    public:
        typedef std::mutex Mutex;
        typedef std::unique_lock<Mutex> Lock;

        FontManager();
        FontManager(FontManager const&) = delete;
        ~FontManager();

        FontManager& operator=(FontManager const&) = delete;

        Lock lock() const;

    private:
        friend class Font;
        std::shared_ptr<FontImpl> getFontImpl(std::string const& file,
                unsigned int face);
        Path textToPath(Lock&, pmr::memory_resource* memory,
                Font const& font, utf8::Utf8View text);
        size_t getCharacterIndex(Lock&, Font const& font,
                utf8::Utf8View text, float fontHeight,
                Vector2f pos) const;
        TextExtents getTextExtents(Lock&, Font const& font,
                utf8::Utf8View text, float height);
        std::vector<PositionedGlyph> getGlyphs(Lock&, Font const& font,
                utf8::Utf8View text, float height) const;
        GlyphBitmap rasterizeGlyph(Lock&, Font const& font,
                uint32_t glyphIndex, int pixelSize, int subpixelBin) const;

        friend class FontImpl;
        void unloadFont(FontImpl const& fontImpl);

    private:
        Path const& getGlyphPath(Lock&, Font const& font,
                unsigned int glyph) const;

    private:
        friend class FontManagerDeferred;
        inline FontManagerDeferred* d() { return deferred_.get(); }
        inline FontManagerDeferred const* d() const { return deferred_.get(); }
        std::shared_ptr<FontManagerDeferred> deferred_;
    };
}

