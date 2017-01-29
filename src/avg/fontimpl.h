#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <string>

namespace avg
{
    class FontManager;

    class FontImpl
    {
    public:
        FontImpl(FontManager& manager, std::string const& file_,
                unsigned long faceIndex);
        ~FontImpl();

    private:
        friend class Font;
        friend class FontManager;
        friend class FontManagerDeferred;
        FontManager& manager_;
        FT_Face face_;
        std::string file_;
        unsigned long faceIndex_;
        float descend_ = 0.0f;
        float ascend_ = 0.0f;
        float linegap_ = 0.0f;
    };
}

