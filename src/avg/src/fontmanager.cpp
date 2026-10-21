#include "fontmanager.h"

#include "textextents.h"
#include "positionedglyph.h"
#include "glyphbitmap.h"
#include "font.h"
#include "fontimpl.h"
#include "path.h"
#include "pathbuilder.h"

#include "debug.h"

#include <utf8/utf8.h>

#include <btl/lrucache.h>

#include <pmr/new_delete_resource.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_CACHE_H
#include FT_OUTLINE_H
#include FT_SIZES_H

#include <algorithm>
#include <map>
#include <list>
#include <mutex>
#include <stdexcept>
#include <vector>

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
        explicit Glyph(pmr::memory_resource* memory) :
            path(memory)
        {
        }

        Path path;
        float advanceX = 0.0f;
        Vector2f bearing;
        Vector2f size;
    };

    // One glyph as the walk over a text visits it: its pen origin and line
    // in unscaled font units, y up
    struct PenGlyph
    {
        unsigned int glyphIndex;
        Glyph const& glyph;
        Vector2f pen;
        size_t line;
    };

    // Where a walk ended: the pen after the last glyph or newline, and the
    // number of lines the text spans
    struct GlyphWalk
    {
        Vector2f pen;
        size_t lines;
    };

    struct GlyphOutline
    {
        PathBuilder pathSpec;
    };

    constexpr size_t maxRasterSizes = 16;

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

    // Visits every glyph of the text in pen order, calling
    // onGlyph(PenGlyph const&) -> bool; false stops the walk.
    template <class TOnGlyph>
    GlyphWalk walkGlyphs(Lock& lock, Font const& font, utf8::Utf8View text,
            TOnGlyph&& onGlyph) const;

    // Activates the face size that scales one unit of height to pixelSize
    // pixels. The metric size must be active again before getGlyph reads
    // the face.
    void activateRasterSize(Lock& lock, FontImpl const& font,
            int pixelSize) const;

    pmr::memory_resource* memory_;

    mutable FontManager::Mutex mutex_;
    std::map<FontDesc, std::weak_ptr<FontImpl>> fonts_;
    mutable btl::LruCache<GlyphDesc, Glyph> cache_;

    FT_Library freetypeLibrary_;
};

FontManagerDeferred::FontManagerDeferred() :
    memory_(pmr::new_delete_resource()),
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

    FT_GlyphSlot slot = font.d()->face_->glyph;
    FT_Error error = FT_Load_Glyph(font.d()->face_, glyphIndex,
            FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
    if (error)
        throw std::runtime_error("Unable to load glyph");

    Glyph glyph(memory_);
    glyph.advanceX = (float)slot->advance.x / 6400.0f;
    glyph.bearing[0] = (float)slot->metrics.horiBearingX / 6400.0f;
    glyph.bearing[1] = (float)slot->metrics.horiBearingY / 6400.0f;
    glyph.size[0] = (float)slot->metrics.width / 6400.0f;
    glyph.size[1] = (float)slot->metrics.height / 6400.0f;

    GlyphOutline outline { memory_ };
    FT_Outline_Funcs funcs;
    funcs.move_to = &outlineMoveTo;
    funcs.line_to = &outlineLineTo;
    funcs.conic_to = &outlineConicTo;
    funcs.cubic_to = &outlineCubicTo;
    funcs.shift = 0;
    funcs.delta = 0;

    error = FT_Outline_Decompose(&slot->outline, &funcs,
            static_cast<void*>(&outline));
    if (error)
        throw std::runtime_error("Unable to decompose glyph outline");

    glyph.path = std::move(outline.pathSpec).build();

    auto j = cache_.insert(std::make_pair(desc, std::move(glyph)));

    return j->second;
}

template <class TOnGlyph>
GlyphWalk FontManagerDeferred::walkGlyphs(Lock& lock, Font const& font,
        utf8::Utf8View text, TOnGlyph&& onGlyph) const
{
    float const lineHeight =
        (float)font.d()->face_->size->metrics.height / 6400.0f;

    GlyphWalk walk { Vector2f(0.0f, 0.0f), 1 };

    for (auto c : text)
    {
        if (c == '\n')
        {
            walk.pen = Vector2f(0.0f, walk.pen[1] - lineHeight);
            ++walk.lines;
            continue;
        }

        unsigned int glyphIndex = FT_Get_Char_Index(font.d()->face_, c);
        Glyph const& glyph = getGlyph(lock, font, glyphIndex);

        if (!onGlyph(PenGlyph { glyphIndex, glyph, walk.pen, walk.lines - 1 }))
            break;

        walk.pen[0] += glyph.advanceX;
    }

    return walk;
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

    FontDesc desc(file, face);
    auto i = d()->fonts_.find(desc);
    if (i != d()->fonts_.end())
    {
        if (auto existing = i->second.lock())
            return existing;
    }

    // ~FontImpl relocks the mutex, so no FontImpl may exist on a failure
    // path while it is held.
    FT_Face ftFace;
    if (FT_New_Face(d()->freetypeLibrary_, file.c_str(), face, &ftFace) != 0)
    {
        throw std::runtime_error("Unable to create new face from file: "
                + file);
    }

    FT_Set_Char_Size(ftFace, 0, 6400, 0, 100);

    auto& entry = d()->fonts_[desc];
    auto ptr = std::make_shared<FontImpl>(*this, file, face, ftFace);
    entry = ptr;

    return ptr;
}

Path FontManager::textToPath(Lock& lock, pmr::memory_resource* memory,
        Font const& font, utf8::Utf8View text)
{
    Path result(memory);

    d()->walkGlyphs(lock, font, text, [&](PenGlyph const& g)
    {
        result += Transform().setTranslation(g.pen) * g.glyph.path;
        return true;
    });

    return result;
}

size_t FontManager::getCharacterIndex(Lock& lock, Font const& font,
        utf8::Utf8View text, float fontHeight, ase::Vector2f pos) const
{
    size_t result = 0;
    float const lineHeight =
        (float)font.d()->face_->size->metrics.height / 6400.0f;

    d()->walkGlyphs(lock, font, text, [&](PenGlyph const& g)
    {
        float left = g.pen[0] * fontHeight;
        float right = (g.pen[0] + g.glyph.advanceX) * fontHeight;
        float bottom = g.pen[1] * fontHeight;
        float top = (g.pen[1] + lineHeight) * fontHeight;

        if (bottom <= pos[1] && pos[1] < top && left <= pos[0]
                && pos[0] < right)
        {
            return false;
        }

        ++result;
        return true;
    });

    return result;
}

TextExtents FontManager::getTextExtents(Lock& lock, Font const& font,
        utf8::Utf8View text, float fontHeight)
{
    float const lineHeight =
        (float)font.d()->face_->size->metrics.height / 6400.0f;

    float w = 0.0f;
    float h = lineHeight;
    float xBearing = 0.0f;
    float yBearing = 0.0f;
    bool first = true;
    size_t line = 0;

    GlyphWalk walk = d()->walkGlyphs(lock, font, text,
            [&](PenGlyph const& g)
    {
        for (; line < g.line; ++line)
            h += lineHeight;

        float x = g.pen[0] + g.glyph.advanceX;
        float y = g.pen[1];

        if (first)
        {
            xBearing = g.glyph.bearing[0];
            yBearing = g.glyph.bearing[1];
        }
        else
        {
            if (x == 0.0f)
                xBearing = std::min(xBearing, g.glyph.bearing[0]);

            if (y == 0.0f)
                yBearing = std::max(yBearing, g.glyph.bearing[1]);
        }

        w = std::max(w, -xBearing + x + g.glyph.size[0] + g.glyph.bearing[0]);
        h = std::max(h, -yBearing + y + g.glyph.size[1] + g.glyph.bearing[1]);

        first = false;
        return true;
    });

    for (; line + 1 < walk.lines; ++line)
        h += lineHeight;

    return TextExtents{
        Vector2f(xBearing * fontHeight, yBearing * fontHeight),
        Vector2f(w * fontHeight, h * fontHeight),
        Vector2f(walk.pen[0] * fontHeight, walk.pen[1] * fontHeight)
    };
}

std::vector<PositionedGlyph> FontManager::getGlyphs(Lock& lock,
        Font const& font, utf8::Utf8View text, float height) const
{
    std::vector<PositionedGlyph> result;

    d()->walkGlyphs(lock, font, text, [&](PenGlyph const& g)
    {
        result.push_back(PositionedGlyph { g.glyphIndex, g.pen * height });
        return true;
    });

    return result;
}

GlyphBitmap FontManager::rasterizeGlyph(Lock& lock, Font const& font,
        uint32_t glyphIndex, int pixelSize, int subpixelBin) const
{
    if (pixelSize < 1)
        throw std::invalid_argument("Glyph pixel size must be positive");

    if (subpixelBin < 0 || subpixelBin >= subpixelBins)
        throw std::invalid_argument("Glyph subpixel bin out of range");

    FontImpl const& impl = *font.d();
    FT_Face face = impl.face_;

    struct MetricSizeRestorer
    {
        FT_Size size;

        ~MetricSizeRestorer()
        {
            FT_Activate_Size(size);
        }
    } restorer { impl.metricSize_ };

    d()->activateRasterSize(lock, impl, pixelSize);

    FT_Error error = FT_Load_Glyph(face, glyphIndex,
            FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
    if (error)
        throw std::runtime_error("Unable to load glyph");

    FT_Outline_Translate(&face->glyph->outline,
            subpixelBin * 64 / subpixelBins, 0);

    error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    if (error)
        throw std::runtime_error("Unable to render glyph");

    FT_Bitmap const& bitmap = face->glyph->bitmap;
    if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY || bitmap.num_grays != 256)
        throw std::runtime_error("Unexpected glyph bitmap format");

    GlyphBitmap result;
    result.width = (int)bitmap.width;
    result.height = (int)bitmap.rows;
    result.bearingX = face->glyph->bitmap_left;
    result.bearingY = face->glyph->bitmap_top;
    result.coverage.resize((size_t)result.width * result.height);

    for (int row = 0; row < result.height; ++row)
    {
        unsigned char const* src = bitmap.buffer
            + (ptrdiff_t)row * bitmap.pitch;
        std::copy(src, src + result.width,
                result.coverage.begin() + (ptrdiff_t)row * result.width);
    }

    return result;
}

void FontManager::unloadFont(FontImpl const& fontImpl)
{
    auto locked = lock();

    for (auto const& rasterSize : fontImpl.rasterSizes_)
        FT_Done_Size(rasterSize.size);

    auto i = d()->fonts_.find(FontDesc(fontImpl.file_, fontImpl.faceIndex_));
    if (i == d()->fonts_.end() || !i->second.expired())
        return;

    d()->fonts_.erase(i);
}

void FontManagerDeferred::activateRasterSize(Lock& /*lock*/,
        FontImpl const& font, int pixelSize) const
{
    auto& sizes = font.rasterSizes_;

    auto i = std::find_if(sizes.begin(), sizes.end(),
            [&](RasterSize const& s)
            {
                return s.pixelSize == pixelSize;
            });

    if (i == sizes.end())
    {
        if (sizes.size() >= maxRasterSizes)
        {
            auto oldest = std::min_element(sizes.begin(), sizes.end(),
                    [](RasterSize const& a, RasterSize const& b)
                    {
                        return a.lastUse < b.lastUse;
                    });

            FT_Done_Size(oldest->size);
            sizes.erase(oldest);
        }

        FT_Size size = nullptr;
        if (FT_New_Size(font.face_, &size))
            throw std::runtime_error("Unable to create font size");

        FT_Activate_Size(size);

        // One unit of height is 1/100 pixel at the metric size (its 26.6
        // coordinates are divided by 6400), so this is the metric scale
        // times pixelSize / 100.
        FT_Size_RequestRec request = {};
        request.type = FT_SIZE_REQUEST_TYPE_SCALES;
        request.width = FT_MulDiv(font.metricSize_->metrics.x_scale,
                pixelSize * 64, 6400);
        request.height = FT_MulDiv(font.metricSize_->metrics.y_scale,
                pixelSize * 64, 6400);

        if (FT_Request_Size(font.face_, &request))
        {
            FT_Done_Size(size);
            throw std::runtime_error("Unable to set font size");
        }

        sizes.push_back(RasterSize { pixelSize, size, 0 });
        i = std::prev(sizes.end());
    }
    else
    {
        FT_Activate_Size(i->size);
    }

    i->lastUse = ++font.rasterUses_;
}

Path const& FontManager::getGlyphPath(Lock& lock, Font const& font,
        unsigned int glyphIndex) const
{
    return d()->getGlyph(lock, font, glyphIndex).path;
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

