#include "image.h"

namespace avg
{

Image::Image(ase::Texture texture, RowOrder rowOrder) :
    texture_(std::move(texture)),
    size_(texture_.getSize()),
    rowOrder_(rowOrder)
{
}

ase::Texture const& Image::getTexture() const noexcept
{
    return texture_;
}

ase::Vector2i Image::getSize() const noexcept
{
    return size_;
}

Image::RowOrder Image::getRowOrder() const noexcept
{
    return rowOrder_;
}

bool Image::operator==(Image const& rhs) const noexcept
{
    return texture_ == rhs.texture_ && rowOrder_ == rhs.rowOrder_;
}

bool Image::operator!=(Image const& rhs) const noexcept
{
    return !(*this == rhs);
}

bool Image::operator<(Image const& rhs) const noexcept
{
    if (texture_ != rhs.texture_)
        return texture_ < rhs.texture_;

    return rowOrder_ < rhs.rowOrder_;
}

} // namespace avg
