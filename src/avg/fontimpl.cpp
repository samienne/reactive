#include "fontimpl.h"

#include "fontmanager.h"

namespace avg
{

FontImpl::FontImpl(FontManager& manager, std::string const& file,
        unsigned long faceIndex) :
    manager_(manager),
    file_(file),
    faceIndex_(faceIndex)
{
}

FontImpl::~FontImpl()
{
    manager_.unloadFont(*this);
}

} // namespace

