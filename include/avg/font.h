#pragma once

#include <ase/vector.h>

#include <btl/hash.h>
#include <btl/visibility.h>

#include <utf8/utf8.h>

#include <string>
#include <memory>

namespace avg
{
    class FontImpl;
    struct TextExtents;
    class Path;
    class FontManager;

    class BTL_VISIBLE Font
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
        Path textToPath(utf8::Utf8View text, float height,
                ase::Vector2f pos, Hinting hinting = HINT_NONE) const;
        size_t getCharacterIndex(utf8::Utf8View, float fontHeight,
                ase::Vector2f pos) const;

        float getDescender(float height) const;
        float getAscender(float height) const;
        float getLinegap(float height) const;
        float getLinespace(float height) const;

        bool operator==(Font const& rhs) const;
        bool operator!=(Font const& rhs) const;
        bool operator<(Font const& rhs) const;
        bool operator>(Font const& rhs) const;

        bool isEmpty() const;

        BTL_VISIBLE friend std::ostream& operator<<(std::ostream& stream, Font const& f);

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

