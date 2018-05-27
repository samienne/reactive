#pragma once

#include "font.h"
#include "transform.h"
#include "pen.h"
#include "brush.h"
#include "vector.h"

#include <btl/hash.h>
#include <btl/option.h>
#include <btl/visibility.h>

#include <string>

namespace avg
{
    class BTL_VISIBLE TextEntry
    {
    public:
        TextEntry(Font const& font, Transform const& transform,
                std::string const& text,
                btl::option<Brush> const& brush = btl::none,
                btl::option<Pen> const& pen = btl::none);
        TextEntry(TextEntry const&) = default;
        TextEntry(TextEntry&&) = default;
        ~TextEntry();

        TextEntry& operator=(TextEntry const&) = default;
        TextEntry& operator=(TextEntry&&) = default;

        bool operator==(TextEntry const& rhs) const;
        bool operator!=(TextEntry const& rhs) const;

        TextEntry operator+(Vector2f offset) const;
        TextEntry operator*(float scale) const;
        TextEntry& operator+=(Vector2f offset);
        TextEntry& operator*=(float scale);

        Font const& getFont() const;
        Transform const& getTransform() const;
        std::string const& getText() const;
        btl::option<Brush> const& getBrush() const;
        btl::option<Pen> const& getPen() const;

        TextEntry transformR(avg::Transform const& t) const;

        friend TextEntry operator*(Transform const& t, TextEntry const& text);

        friend std::ostream& operator<<(std::ostream& stream,
                TextEntry const& t)
        {
            return stream << "TextEntry{" << t.font_ << ", "
                << t.transform_ << ", text: \"" << t.text_ << "\", brush: "
                << t.brush_ << ", pen: " << t.pen_ << "}";
        }

        template <class THash>
        friend void hash_append(THash& h, TextEntry const& text) noexcept
        {
            using btl::hash_append;
            hash_append(h, text.font_);
            hash_append(h, text.transform_);
            hash_append(h, text.text_);
            hash_append(h, text.brush_);
            hash_append(h, text.pen_);
        }

    private:
        Font font_;
        Transform transform_;
        std::string text_;
        btl::option<Brush> brush_;
        btl::option<Pen> pen_;
    };
}

