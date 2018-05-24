#pragma once

#include "vector.h"

#include <utf8/utf8.h>

#include <mutex>
#include <memory>

namespace avg
{
    class FontImpl;
    class Font;
    class Path;
    struct TextExtents;
    class FontManagerDeferred;

    class FontManager
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
        Path textToPath(Lock&, Font const& font, utf8::Utf8View text);
        size_t getCharacterIndex(Lock&, Font const& font,
                utf8::Utf8View text, float fontHeight,
                Vector2f pos) const;
        TextExtents getTextExtents(Lock&, Font const& font,
                utf8::Utf8View text, float height);

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

