#include "fontmanager.h"

#include "textextents.h"
#include "font.h"
#include "fontimpl.h"
#include "path.h"
#include "pathspec.h"

#include "debug.h"

#include <utf8/utf8.h>

#include <btl/lrucache.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_CACHE_H
#include FT_OUTLINE_H

#include <map>
#include <list>
#include <mutex>

namespace avg
{

namespace
{
    struct GlyphDesc
    {
        GlyphDesc(FontImpl const* aFont, unsigned int aGlyph) :
            font(aFont), glyph(aGlyph)
        {
        }

        bool operator<(GlyphDesc const& rhs) const
        {
            if (font != rhs.font)
                return font < rhs.font;

            return glyph < rhs.glyph;
        }

        FontImpl const* font;
        unsigned int glyph;
    };

    struct FontDesc
    {
        FontDesc() :
            file(), face(0)
        {
        }

        FontDesc(FontDesc const&) = default;

        FontDesc& operator=(FontDesc const&) = default;

        FontDesc(std::string const& aFile, unsigned int aFace) :
                file(aFile), face(aFace)
        {
        }

        bool operator<(FontDesc const& rhs) const
        {
            if (face != rhs.face)
                return face < rhs.face;

            return file < rhs.file;
        }

        std::string file;
        unsigned int face;
    };

    struct Glyph
    {
        Path path;
        unsigned int loadedGlyph;
        float advanceX;
        ase::Vector2f bearing;
        ase::Vector2f size;
    };

    struct GlyphOutline
    {
        PathSpec pathSpec;
    };

    int outlineMoveTo(FT_Vector const* to, void* user);
    int outlineLineTo(FT_Vector const* to, void* user);
    int outlineConicTo(FT_Vector const* control,
            FT_Vector const* to, void* user);
    int outlineCubicTo(FT_Vector const* control1,
            FT_Vector const* control2, FT_Vector const* to, void* user);

}

class FontManagerDeferred
{
public:
    typedef std::mutex Mutex;
    typedef std::unique_lock<Mutex> Lock;

    FontManagerDeferred();
    ~FontManagerDeferred();

    Glyph& getGlyph(Lock& lock, Font const& font, unsigned int glyph) const;
    Glyph& getGlyphWithOutline(Lock& lock, Font const& font,
            unsigned int glyph) const;

    mutable FontManager::Mutex mutex_;
    std::map<FontDesc, std::weak_ptr<FontImpl>> fonts_;
    mutable btl::LruCache<GlyphDesc, Glyph> cache_;

    FT_Library freetypeLibrary_;
};

FontManagerDeferred::FontManagerDeferred() :
    cache_(200)
{
}

FontManagerDeferred::~FontManagerDeferred()
{
}

Glyph& FontManagerDeferred::getGlyph(Lock& /*lock*/, Font const& font,
        unsigned int glyphIndex) const
{
    GlyphDesc desc(font.d(), glyphIndex);
    auto i = cache_.find(desc);
    if (i != cache_.end())
        return i->second;

    Glyph glyph;

    // Todo: fill glyph data
    FT_Load_Glyph(font.d()->face_, glyphIndex, FT_LOAD_NO_HINTING |
            FT_LOAD_NO_BITMAP);

    glyph.advanceX = (float)font.d()->face_->glyph->advance.x / 6400.0f;
    glyph.loadedGlyph = glyphIndex;
    glyph.bearing[0] = (float)font.d()->face_->glyph->metrics.horiBearingX
        / 6400.0f;
    glyph.bearing[1] = (float)font.d()->face_->glyph->metrics.horiBearingY
        / 6400.0f;
    glyph.size[0] = (float)font.d()->face_->glyph->metrics.width / 6400.0f;
    glyph.size[1] = (float)font.d()->face_->glyph->metrics.height / 6400.0f;

    auto j = cache_.insert(std::make_pair(desc, glyph));

    return j->second;
}

Glyph& FontManagerDeferred::getGlyphWithOutline(Lock& lock, Font const& font,
        unsigned int glyphIndex) const
{
    Glyph& glyph = getGlyph(lock, font, glyphIndex);
    if (!glyph.path.isEmpty())
        return glyph;

    if (glyph.loadedGlyph != glyphIndex)
    {
        FT_Error error = FT_Load_Glyph(font.d()->face_, glyphIndex,
                FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
        if (error)
            throw std::runtime_error("Unable to load glyph");
    }

    GlyphOutline outline;
    FT_Outline_Funcs funcs;
    funcs.move_to = &outlineMoveTo;
    funcs.line_to = &outlineLineTo;
    funcs.conic_to = &outlineConicTo;
    funcs.cubic_to = &outlineCubicTo;
    funcs.shift = 0;
    funcs.delta = 0;

    FT_Error error = FT_Outline_Decompose(&font.d()->face_->glyph->outline,
            &funcs, static_cast<void*>(&outline));
    if (error)
        throw std::runtime_error("Unable to decompose glyph outline");

    /*DBG("%1 %2", glyphIndex, font.d()->face_->glyph->outline.flags
            & (FT_OUTLINE_EVEN_ODD_FILL | FT_OUTLINE_REVERSE_FILL));*/

    glyph.path = Path(std::move(outline.pathSpec));
    return glyph;
}

FontManager::FontManager() :
    deferred_(std::make_shared<FontManagerDeferred>())
{
    FT_Error error;

    error = FT_Init_FreeType(&d()->freetypeLibrary_);
    if (error)
        throw std::runtime_error("Unable to initialize FreeType library");
}

FontManager::~FontManager()
{
    FT_Done_FreeType(d()->freetypeLibrary_);
}

FontManager::Lock FontManager::lock() const
{
    return Lock(d()->mutex_);
}

std::shared_ptr<FontImpl> FontManager::getFontImpl(std::string const& file,
        unsigned int face)
{
    auto locked = lock();

    auto i = d()->fonts_.find(FontDesc(file, face));
    if (i != d()->fonts_.end())
    {
        return i->second.lock();
    }

    auto ptr = std::make_shared<FontImpl>(*this, file, face);

    if (FT_New_Face(d()->freetypeLibrary_, file.c_str(),
                face, &ptr->face_) != 0)
    {
        throw std::runtime_error("Unable to create new face from file: "
                + file);
    }

    FT_Set_Char_Size(ptr->face_, 0, 6400, 0, 100);

    ptr->descend_ = (float)ptr->face_->descender / 6400.0f;
    ptr->ascend_ = (float)ptr->face_->ascender / 6400.0f;
    ptr->linegap_ = (float)ptr->face_->size->metrics.height / 6400.0f;

    d()->fonts_.insert(std::make_pair(FontDesc(file, face), ptr));

    return ptr;
}

Path FontManager::textToPath(Lock& lock, Font const& font,
        utf8::Utf8View text)
{
    Path result;
    Transform transform;
    float x = 0.0;
    float y = 0.0;

    //for (auto i = text.begin(); i != text.end(); ++i)
    for (auto c : text)
    {
        if (c == '\n')
        {
            y -= (float)font.d()->face_->size->metrics.height / 6400.0f;
            transform = std::move(transform).setTranslation(x, y);
            continue;
        }

        unsigned int glyphIndex = FT_Get_Char_Index(font.d()->face_, c);
        Glyph& glyph = d()->getGlyphWithOutline(lock, font, glyphIndex);

        result += transform * glyph.path;
        transform = std::move(transform)
            .translate(Vector2f(glyph.advanceX, 0.0f));
    }

    return result;
}

size_t FontManager::getCharacterIndex(Lock& lock, Font const& font,
        utf8::Utf8View text, float fontHeight, ase::Vector2f pos) const
{
    size_t result = 0;

    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float xBearing = 0.0f;
    float yBearing = 0.0f;

    float yStep = (float)font.d()->face_->size->metrics.height / 6400.0f;
    bool first = true;

    //for (auto i = text.begin(); i != text.end(); ++i)
    for (auto c : text)
    {
        if (c == '\n' || first)
        {
            h += yStep;
            x = 0.0f;

            if (!first)
            {
                y -= yStep;
                continue;
            }
        }

        unsigned int glyphIndex = FT_Get_Char_Index(font.d()->face_, c);
        Glyph& glyph = d()->getGlyphWithOutline(lock, font, glyphIndex);

        auto oldX = x;
        x += glyph.advanceX;

        auto nextY = y + yStep;

        if ((y * fontHeight) <= pos[1] && pos[1] < (nextY * fontHeight) &&
                (oldX * fontHeight) <= pos[0] && pos[0] < (x * fontHeight))
        {
            return result;
        }

        ++result;

        if (first)
        {
            xBearing = glyph.bearing[0];
            yBearing = glyph.bearing[1];
        }
        else
        {
            if (x == 0.0f)
                xBearing = std::min(xBearing, glyph.bearing[0]);

            if (y == 0.0f)
                yBearing = std::max(yBearing, glyph.bearing[1]);
        }

        w = std::max(w, -xBearing + x + glyph.size[0] + glyph.bearing[0]);
        h = std::max(h, -yBearing + y + glyph.size[1] + glyph.bearing[1]);

        first = false;
    }

    return result;
}

TextExtents FontManager::getTextExtents(Lock& lock, Font const& font,
        utf8::Utf8View text, float fontHeight)
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float xBearing = 0.0f;
    float yBearing = 0.0f;

    float yStep = (float)font.d()->face_->size->metrics.height / 6400.0f;
    bool first = true;

    //for (auto i = text.begin(); i != text.end(); ++i)
    for (auto c : text)
    {
        if (c == '\n' || first)
        {
            h += yStep;
            x = 0.0f;

            if (!first)
            {
                y -= yStep;
                continue;
            }
        }

        unsigned int glyphIndex = FT_Get_Char_Index(font.d()->face_, c);
        Glyph& glyph = d()->getGlyphWithOutline(lock, font, glyphIndex);

        x += glyph.advanceX;

        if (first)
        {
            xBearing = glyph.bearing[0];
            yBearing = glyph.bearing[1];
        }
        else
        {
            if (x == 0.0f)
                xBearing = std::min(xBearing, glyph.bearing[0]);

            if (y == 0.0f)
                yBearing = std::max(yBearing, glyph.bearing[1]);
        }

        w = std::max(w, -xBearing + x + glyph.size[0] + glyph.bearing[0]);
        h = std::max(h, -yBearing + y + glyph.size[1] + glyph.bearing[1]);

        first = false;
    }

    return TextExtents{
        Vector2f(xBearing * fontHeight, yBearing * fontHeight),
        Vector2f(w * fontHeight, h * fontHeight),
        Vector2f(x * fontHeight, y * fontHeight)
    };
}

void FontManager::unloadFont(FontImpl const& fontImpl)
{
    auto locked = lock();

    auto i = d()->fonts_.find(FontDesc(fontImpl.file_, fontImpl.faceIndex_));
    if (i == d()->fonts_.end())
        return;

    d()->fonts_.erase(i);
}

Path const& FontManager::getGlyphPath(Lock& lock, Font const& font,
        unsigned int glyphIndex) const
{
    Glyph& glyph = d()->getGlyphWithOutline(lock, font, glyphIndex);
    return glyph.path;
}


namespace
{

int outlineMoveTo(FT_Vector const* to, void* user)
{
    GlyphOutline& outline = *reinterpret_cast<GlyphOutline*>(user);

    outline.pathSpec = std::move(outline.pathSpec)
        .start(ase::Vector2f((float)to->x / 6400.0f, (float)to->y / 6400.0f));

    return 0;
}

int outlineLineTo(FT_Vector const* to, void* user)
{
    GlyphOutline& outline = *reinterpret_cast<GlyphOutline*>(user);
    outline.pathSpec = std::move(outline.pathSpec)
        .lineTo(ase::Vector2f((float)to->x / 6400.0f, (float)to->y / 6400.0f));

    return 0;
}

int outlineConicTo(FT_Vector const* control,
        FT_Vector const* to, void* user)
{
    GlyphOutline& outline = *reinterpret_cast<GlyphOutline*>(user);
    outline.pathSpec = std::move(outline.pathSpec)
        .conicTo(
                ase::Vector2f((float)control->x / 6400.0f,
                    (float)control->y / 6400.0f),
                ase::Vector2f((float)to->x / 6400.0f, (float)to->y / 6400.0f));

    return 0;
}

int outlineCubicTo(FT_Vector const* control1,
        FT_Vector const* control2, FT_Vector const* to, void* user)
{
    GlyphOutline& outline = *reinterpret_cast<GlyphOutline*>(user);

    outline.pathSpec = std::move(outline.pathSpec)
        .cubicTo(ase::Vector2f((float)control1->x / 6400.0f,
                    (float)control1->y / 6400.0f),
                ase::Vector2f((float)control2->x / 6400.0f,
                    (float)control2->y / 6400.0f),
                ase::Vector2f((float)to->x / 6400.0f, (float)to->y / 6400.0f));

    return 0;
}

} // namespace
} // namespace

