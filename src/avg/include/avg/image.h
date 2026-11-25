#pragma once

#include "avgvisibility.h"

#include <ase/texture.h>
#include <ase/vector.h>

#include <btl/hash.h>

#include <cstdint>
#include <ostream>

namespace avg
{
    /**
     * @brief A texture to paint shapes with, through a `Brush`.
     *
     * The texture holds premultiplied colour in an sRGB format, such as
     * `ase::FORMAT_SRGBA`: `Image(context.makeTexture(size, FORMAT_SRGBA))`.
     * A brush stretches the whole image over the bounding box of the shape
     * it fills.
     */
    class AVG_EXPORT Image final
    {
    public:
        /**
         * @brief Which end of the image the texture's first row is.
         *
         * Uploaded pixels usually start with the top row; a texture
         * rendered to through an ase framebuffer starts with the bottom
         * row.
         */
        enum class RowOrder
        {
            TopFirst,
            BottomFirst
        };

        explicit Image(ase::Texture texture,
                RowOrder rowOrder = RowOrder::TopFirst);

        ase::Texture const& getTexture() const noexcept;

        /**
         * @brief The texture's size in pixels.
         */
        ase::Vector2i getSize() const noexcept;
        RowOrder getRowOrder() const noexcept;

        bool operator==(Image const& rhs) const noexcept;
        bool operator!=(Image const& rhs) const noexcept;
        bool operator<(Image const& rhs) const noexcept;

        friend std::ostream& operator<<(std::ostream& stream, Image const& i)
        {
            return stream << "Image{" << i.size_[0] << "x" << i.size_[1]
                << "}";
        }

        template <class THash>
        friend void hash_append(THash& h, Image const& image) noexcept
        {
            using btl::hash_append;
            hash_append(h, image.size_[0]);
            hash_append(h, image.size_[1]);
            hash_append(h, static_cast<int32_t>(image.rowOrder_));
        }

    private:
        ase::Texture texture_;
        ase::Vector2i size_;
        RowOrder rowOrder_;
    };
} // namespace avg
