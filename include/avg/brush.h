#pragma once

#include "color.h"
#include "transform.h"

#include <btl/hash.h>

#include <ostream>

namespace avg
{
    class Brush final
    {
    public:
        Brush(Color const& color = Color());
        Brush(Brush const&) = default;
        Brush(Brush&&) = default;
        ~Brush();

        Brush& operator=(Brush const&) = default;
        Brush& operator=(Brush&&) = default;

        Color const& getColor() const noexcept;

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
            return stream << "Brush{" << b.color_ << "}";
        }

        template <class THash>
        friend void hash_append(THash& h, Brush const& brush) noexcept
        {
            using btl::hash_append;
            hash_append(h, brush.color_);
        }

    private:
        Color color_;
    };

    inline Brush::Brush(Color const& color) :
        color_(color)
    {
    }

    inline Brush::~Brush()
    {
    }

    inline Color const& Brush::getColor() const noexcept
    {
        return color_;
    }

    inline bool Brush::operator==(Brush const& rhs) const noexcept
    {
        return color_ == rhs.color_;
    }

    inline bool Brush::operator!=(Brush const& rhs) const noexcept
    {
        return color_ != rhs.color_;
    }

    inline bool Brush::operator<(Brush const& rhs) const noexcept
    {
        return color_ < rhs.color_;
    }

    inline bool Brush::operator>(Brush const& rhs) const noexcept
    {
        return color_ > rhs.color_;
    }

    inline Brush Brush::operator*(float) const noexcept
    {
        return *this;
    }
}

