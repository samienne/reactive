#pragma once

#include "rect.h"
#include "obb.h"
#include "font.h"
#include "transform.h"
#include "pen.h"
#include "brush.h"
#include "vector.h"
#include "avgvisibility.h"

#include <btl/optional.h>
#include <btl/hash.h>

#include <string>
#include <optional>

namespace avg
{
    class AVG_EXPORT TextEntry
    {
    public:
        TextEntry(Font const& font, Transform const& transform,
                std::string const& text,
                std::optional<Brush> const& brush = std::nullopt,
                std::optional<Pen> const& pen = std::nullopt);
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
        std::optional<Brush> const& getBrush() const;
        std::optional<Pen> const& getPen() const;

        Rect getControlBb() const;
        Obb getControlObb() const;

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
        std::optional<Brush> brush_;
        std::optional<Pen> pen_;
    };
}

