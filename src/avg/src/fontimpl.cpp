#include "fontimpl.h"

#include "fontmanager.h"

namespace avg
{

FontImpl::FontImpl(FontManager& manager, std::string const& file,
        unsigned long faceIndex, FT_Face face) :
    manager_(manager),
    face_(face),
    file_(file),
    faceIndex_(faceIndex),
    descend_((float)face->descender / 6400.0f),
    ascend_((float)face->ascender / 6400.0f),
    linegap_((float)face->size->metrics.height / 6400.0f)
{
}

FontImpl::~FontImpl()
{
    manager_.unloadFont(*this);
}

} // namespace

