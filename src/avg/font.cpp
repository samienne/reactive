#include "font.h"

#include "fontimpl.h"
#include "fontmanager.h"

#include "textextents.h"
#include "path.h"

#include "transform.h"

#include "debug.h"

#include <stdexcept>

namespace avg
{

static FontManager manager;

Font::Font(std::string const& file, unsigned int face) :
    deferred_(manager.getFontImpl(file, face))
{
}

Font::~Font()
{
}

TextExtents Font::getTextExtents(utf8::Utf8View text, float size) const
{
    if (text.empty())
        return TextExtents{};

    FontManager::Lock lock(manager.lock());
    return d()->manager_.getTextExtents(lock, *this, text, size);
}

Path Font::textToPath(utf8::Utf8View text, float height,
        ase::Vector2f pos, Hinting /*hinting*/) const
{
    if (!d())
        return Path();

    auto transform = avg::Transform()
        .setTranslation(pos)
        .setScale(height);

    FontManager::Lock lock(manager.lock());
    return transform * d()->manager_.textToPath(lock, *this, text);
}

size_t Font::getCharacterIndex(utf8::Utf8View text, float fontHeight,
        ase::Vector2f pos) const
{
    FontManager::Lock lock(manager.lock());
    return d()->manager_.getCharacterIndex(lock, *this, text, fontHeight, pos);
}

float Font::getDescender(float height) const
{
    return d()->descend_ * height;
}

float Font::getAscender(float height) const
{
    return d()->ascend_ * height;
}

float Font::getLinegap(float height) const
{
    return d()->linegap_ * height;
}

float Font::getLinespace(float height) const
{
    return getAscender(height) - getDescender(height) + getLinegap(height);
}

bool Font::operator==(Font const& rhs) const
{
    return deferred_ == rhs.deferred_;
}

bool Font::operator!=(Font const& rhs) const
{
    return deferred_ != rhs.deferred_;
}

bool Font::operator<(Font const& rhs) const
{
    return deferred_ < rhs.deferred_;
}

bool Font::operator>(Font const& rhs) const
{
    return deferred_ > rhs.deferred_;
}

bool Font::isEmpty() const
{
    return !deferred_;
}

std::ostream& operator<<(std::ostream& stream, Font const& f)
{
    if (f.isEmpty())
        stream << "Font{empty}" << std::endl;

    return stream << "Font{" << f.deferred_ << "}";
}

} // namespace

