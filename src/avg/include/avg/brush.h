#pragma once

#include "color.h"
#include "image.h"
#include "transform.h"
#include "avgvisibility.h"

#include <btl/hash.h>

#include <optional>
#include <ostream>

namespace avg
{
    class AVG_EXPORT Brush final
    {
    public:
        Brush(Color const& color = Color());

        /**
         * @brief Paint with `image`, stretched over the bounding box of the
         * filled shape and multiplied by `color`.
         *
         * `Brush(Image(texture), Color(1, 1, 1, 0.5f))` paints the texture
         * at half opacity.
         */
        explicit Brush(Image image, Color const& color = Color());
        Brush(Brush const&) = default;
        Brush(Brush&&) = default;
        ~Brush();

        Brush& operator=(Brush const&) = default;
        Brush& operator=(Brush&&) = default;

        Color const& getColor() const noexcept;
        std::optional<Image> const& getImage() const noexcept;

        Brush operator*(float scale) const noexcept;

        bool operator==(Brush const& rhs) const noexcept;
        bool operator!=(Brush const& rhs) const noexcept;
        bool operator<(Brush const& rhs) const noexcept;
        bool operator>(Brush const& rhs) const noexcept;

        friend Brush operator*(Transform const&, Brush const& b) noexcept
        {
            return b;
        }

        friend std::ostream& operator<<(std::ostream& stream, Brush const& b)
        {
            stream << "Brush{" << b.color_;
            if (b.image_)
                stream << ", " << *b.image_;
            return stream << "}";
        }

        template <class THash>
        friend void hash_append(THash& h, Brush const& brush) noexcept
        {
            using btl::hash_append;
            hash_append(h, brush.color_);
            if (brush.image_)
                hash_append(h, *brush.image_);
        }

    private:
        Color color_;
        std::optional<Image> image_;
    };
} // namespace avg
