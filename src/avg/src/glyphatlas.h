#pragma once

#include "atlasallocator.h"

#include <avg/font.h>
#include <avg/avgvisibility.h>

#include <ase/buffer.h>
#include <ase/commandbuffer.h>
#include <ase/rendercontext.h>
#include <ase/texture.h>
#include <ase/vector.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace avg
{
    /**
     * @brief Where a glyph's coverage lives in the atlas texture, and how to
     * place it.
     *
     * rect is in texels of GlyphAtlas::getTexture(), y down from the top
     * row; divide by GlyphAtlas::getTextureSize() for texture coordinates.
     * A glyph that leaves no mark (a space) has a rect of zero size. The
     * bearings are those of Font::rasterizeGlyph: the quad's left edge is
     * bearingX pixels right of the pen origin and its top edge bearingY
     * pixels above the baseline, and it is rect.width by rect.height pixels.
     */
    struct GlyphSlot
    {
        AtlasRect rect;
        int bearingX = 0;
        int bearingY = 0;
    };

    /**
     * @brief A cache of rasterised glyphs in one texture of a render
     * context.
     *
     * Each frame goes beginFrame(), any number of get() calls while the
     * draw commands are built, then endFrame() with the command buffer that
     * uploads the glyphs added this frame; submit that buffer before the
     * draw commands that sample the texture. A glyph fetched during a frame
     * keeps its place until the next beginFrame(). When a new glyph does not
     * fit, get() returns nullopt for every further new glyph of the frame
     * and the caller draws them some other way; the atlas then doubles
     * (up to the maximum size) or, at the maximum, empties, at endFrame().
     *
     * Growing replaces the texture, so the texture object as well as its
     * size can change at endFrame(): read both every frame, after
     * beginFrame(). The texture is single-channel coverage, FORMAT_R8.
     */
    class AVG_EXPORT GlyphAtlas
    {
    public:
        /**
         * @brief Creates an atlas of initialSize by initialSize texels that
         * grows up to maximumSize by maximumSize; both are powers of two.
         */
        explicit GlyphAtlas(ase::RenderContext& context,
                int initialSize = 512,
                int maximumSize = 2048);

        GlyphAtlas(GlyphAtlas const&) = delete;
        GlyphAtlas(GlyphAtlas&&) = default;
        GlyphAtlas& operator=(GlyphAtlas const&) = delete;
        GlyphAtlas& operator=(GlyphAtlas&&) = delete;

        void beginFrame();

        /**
         * @brief The slot of a glyph, rasterising it into the atlas on first
         * use.
         *
         * The arguments are those of Font::rasterizeGlyph. Returns nullopt
         * when the glyph is new and does not fit this frame, or when
         * pixelSize is outside 1 to 65535.
         */
        std::optional<GlyphSlot> get(Font const& font,
                uint32_t glyphIndex,
                int pixelSize,
                int subpixelBin);

        /**
         * @brief Pushes this frame's glyph uploads into commands, then grows
         * or empties the atlas if a glyph did not fit.
         */
        void endFrame(ase::CommandBuffer& commands);

        /**
         * @brief The texture the slots of this frame refer to.
         */
        ase::Texture const& getTexture() const;

        /**
         * @brief The size of getTexture() in texels.
         */
        ase::Vector2i getTextureSize() const;

        /**
         * @brief The number of glyphs in the atlas, blank ones included.
         */
        size_t getCount() const;

    private:
        // The glyph's coverage with its 1 px padding ring, uploaded around
        // rect.
        struct PendingUpload
        {
            AtlasRect rect;
            ase::Buffer coverage;
        };

        void reset(int size);
        void pruneSlots();

        ase::RenderContext& context_;
        int maximumSize_;
        AtlasAllocator allocator_;
        ase::Texture texture_;
        std::unordered_map<GlyphKey, GlyphSlot, GlyphKeyHash> slots_;
        std::unordered_map<void const*, Font> fonts_;
        size_t blankCount_ = 0;
        std::vector<PendingUpload> pending_;
        bool overflowed_ = false;
    };
} // namespace avg
