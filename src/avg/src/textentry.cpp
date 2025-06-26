#include "textentry.h"

#include "textextents.h"

namespace avg
{

TextEntry::TextEntry(Font const& font, Transform const& transform,
    std::string const& text, std::optional<Brush> const& brush,
    std::optional<Pen> const& pen) :
    font_(font),
    transform_(transform),
    text_(text),
    brush_(brush),
    pen_(pen)
{
}

TextEntry::~TextEntry()
{
}

bool TextEntry::operator==(TextEntry const& rhs) const
{
    return font_ == rhs.font_ && transform_ == rhs.transform_
        && text_ == rhs.text_ && brush_ == rhs.brush_ && pen_ == rhs.pen_;
}

bool TextEntry::operator!=(TextEntry const& rhs) const
{
    return font_ != rhs.font_ || transform_ != rhs.transform_
        || text_ != rhs.text_ || brush_ != rhs.brush_ || pen_ != rhs.pen_;
}

TextEntry TextEntry::operator+(Vector2f offset) const
{
    TextEntry t(*this);
    t.transform_ = std::move(t.transform_).translate(offset);
    return t;
}

TextEntry TextEntry::operator*(float scale) const
{
    TextEntry t(font_, transform_, text_);
    t.transform_ = std::move(t.transform_).scale(scale);
    return t;
}

TextEntry& TextEntry::operator+=(Vector2f offset)
{
    transform_ = std::move(transform_).translate(offset);
    return *this;
}

TextEntry& TextEntry::operator*=(float scale)
{
    transform_ = std::move(transform_).scale(scale);
    return *this;
}

Font const& TextEntry::getFont() const
{
    return font_;
}

Transform const& TextEntry::getTransform() const
{
    return transform_;
}

std::string const& TextEntry::getText() const
{
    return text_;
}

std::optional<Brush> const& TextEntry::getBrush() const
{
    return brush_;
}

std::optional<Pen> const& TextEntry::getPen() const
{
    return pen_;
}

Rect TextEntry::getControlBb() const
{
    return getControlObb().getBoundingRect();
}

Obb TextEntry::getControlObb() const
{
    TextExtents te = font_.getTextExtents(utf8::asUtf8(text_), 1.0f);

    return transform_ * Obb(Rect(te.bearing, te.size));
}

TextEntry TextEntry::transformR(avg::Transform const& t) const
{
    TextEntry te(*this);
    te.transform_ = te.transform_ * t;
    return te;
}

TextEntry operator*(Transform const& t, TextEntry const& text)
{
    return TextEntry(text.font_, t * text.transform_, text.text_, text.brush_,
            text.pen_);
}

} // namespace

