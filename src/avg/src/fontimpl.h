#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <string>
#include <vector>

namespace avg
{
    class FontManager;

    // A face size kept for rasterising, with the glyph scale it was made
    // for and when it was last used
    struct RasterSize
    {
        int pixelSize;
        FT_Size size;
        uint64_t lastUse;
    };

    class FontImpl
    {
    public:
        FontImpl(FontManager& manager, std::string const& file,
                unsigned long faceIndex, FT_Face face);
        ~FontImpl();

    private:
        friend class Font;
        friend class FontManager;
        friend class FontManagerDeferred;
        FontManager& manager_;
        FT_Face face_;
        // The size the glyph metrics and outlines are read at; active
        // whenever the manager is not rasterising
        FT_Size metricSize_;
        std::string file_;
        unsigned long faceIndex_;
        float descend_ = 0.0f;
        float ascend_ = 0.0f;
        float linegap_ = 0.0f;
        mutable std::vector<RasterSize> rasterSizes_;
        mutable uint64_t rasterUses_ = 0;
    };
}

