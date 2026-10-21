#include "glyphatlas.h"

#include <avg/glyphbitmap.h>

#include <ase/format.h>

#include <tracy/Tracy.hpp>

#include <algorithm>
#include <cassert>
#include <limits>
#include <vector>

namespace avg
{

namespace
{
    bool isBlank(GlyphSlot const& slot)
    {
        return slot.rect.width == 0 || slot.rect.height == 0;
    }
} // anonymous namespace

GlyphAtlas::GlyphAtlas(ase::RenderContext& context, int initialSize,
        int maximumSize) :
    context_(context),
    maximumSize_(maximumSize),
    allocator_(initialSize, initialSize),
    texture_(context.makeTexture(ase::Vector2i(initialSize, initialSize),
                ase::FORMAT_R8))
{
    assert(initialSize > 0 && initialSize <= maximumSize);
}

void GlyphAtlas::beginFrame()
{
    allocator_.beginFrame();
}

std::optional<GlyphSlot> GlyphAtlas::get(Font const& font,
        uint32_t glyphIndex, int pixelSize, int subpixelBin)
{
    if (pixelSize < 1 || pixelSize > std::numeric_limits<uint16_t>::max())
        return std::nullopt;

    assert(subpixelBin >= 0
            && subpixelBin <= std::numeric_limits<uint8_t>::max());

    GlyphKey key;
    key.font = font.getIdentity();
    key.glyphIndex = glyphIndex;
    key.pixelSize = static_cast<uint16_t>(pixelSize);
    key.subpixelBin = static_cast<uint8_t>(subpixelBin);

    auto i = slots_.find(key);
    if (i != slots_.end()
            && (isBlank(i->second) || allocator_.find(key).has_value()))
    {
        return i->second;
    }

    if (overflowed_)
        return std::nullopt;

    ZoneScopedN("GlyphAtlas rasterise");

    GlyphBitmap bitmap = font.rasterizeGlyph(glyphIndex, pixelSize,
            subpixelBin);

    GlyphSlot slot;
    slot.bearingX = bitmap.bearingX;
    slot.bearingY = bitmap.bearingY;

    if (bitmap.width > 0 && bitmap.height > 0)
    {
        auto rect = allocator_.insert(key, bitmap.width, bitmap.height);
        if (!rect.has_value())
        {
            overflowed_ = true;
            return std::nullopt;
        }

        slot.rect = *rect;

        // An evicted glyph leaves its texels behind, so the 1 px padding
        // ring around this one is written too, as zero: the quad's edge
        // texels would otherwise bleed the previous occupant.
        int const width = bitmap.width + 2;
        int const height = bitmap.height + 2;
        std::vector<uint8_t> padded(
                static_cast<size_t>(width * height), uint8_t(0));
        for (int y = 0; y < bitmap.height; ++y)
        {
            std::copy_n(bitmap.coverage.data() + y * bitmap.width,
                    bitmap.width, padded.data() + (y + 1) * width + 1);
        }

        pending_.push_back(PendingUpload{ *rect, ase::Buffer(padded) });
    }
    else
    {
        ++blankCount_;
    }

    // The key holds the font implementation's address, so the font must
    // outlive every slot keyed by it.
    fonts_.emplace(key.font, font);
    slots_[key] = slot;
    pruneSlots();

    return slot;
}

void GlyphAtlas::endFrame(ase::CommandBuffer& commands)
{
    {
        ZoneScopedN("GlyphAtlas upload");

        for (auto const& upload : pending_)
        {
            commands.pushSubUpload(texture_, upload.coverage,
                    ase::Vector2i(upload.rect.x - 1, upload.rect.y - 1),
                    ase::Vector2i(upload.rect.width + 2,
                        upload.rect.height + 2));
        }

        pending_.clear();
    }

    if (!overflowed_)
        return;

    overflowed_ = false;
    slots_.clear();
    fonts_.clear();
    blankCount_ = 0;

    int size = allocator_.getWidth();
    if (size < maximumSize_)
        reset(size * 2);
    else
        allocator_.clear();
}

ase::Texture const& GlyphAtlas::getTexture() const
{
    return texture_;
}

ase::Vector2i GlyphAtlas::getTextureSize() const
{
    return ase::Vector2i(allocator_.getWidth(), allocator_.getHeight());
}

size_t GlyphAtlas::getCount() const
{
    return allocator_.getCount() + blankCount_;
}

void GlyphAtlas::reset(int size)
{
    allocator_ = AtlasAllocator(size, size);
    texture_ = context_.makeTexture(ase::Vector2i(size, size),
            ase::FORMAT_R8);
}

// The allocator evicts without telling us, so slots_ keeps entries for
// glyphs that are gone until it has grown well past the live count.
void GlyphAtlas::pruneSlots()
{
    if (slots_.size() < 2 * getCount() + 64)
        return;

    for (auto i = slots_.begin(); i != slots_.end();)
    {
        if (!isBlank(i->second) && !allocator_.contains(i->first))
            i = slots_.erase(i);
        else
            ++i;
    }
}

} // namespace avg
