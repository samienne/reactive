#pragma once

#include <btl/hash.h>

#include <array>
#include <ostream>

namespace avg
{
    class Color final
    {
    public:
        inline Color();
        inline Color(float r, float g, float b, float a);
        Color(Color const&) = default;
        Color(Color&&) = default;
        inline ~Color();

        Color& operator=(Color const&) = default;
        Color& operator=(Color&&) = default;

        inline float getRed() const;
        inline float getGreen() const;
        inline float getBlue() const;
        inline float getAlpha() const;

        inline bool operator==(Color const& rhs) const noexcept;
        inline bool operator!=(Color const& rhs) const noexcept;
        inline bool operator<(Color const& rhs) const noexcept;
        inline bool operator>(Color const& rhs) const noexcept;

        inline std::array<float, 4> const& getArray() const;

        friend std::ostream& operator<<(std::ostream& stream, Color const& c)
        {
            return stream << "Color(" << c.getRed() << ", "
                << c.getGreen() << ", " << c.getBlue() <<
                ", " << c.getAlpha() << ")";
        }

    private:
        std::array<float, 4> colors_;
    };

    Color::Color()
    {
        colors_[0] = 1.0f;
        colors_[1] = 1.0f;
        colors_[2] = 1.0f;
        colors_[3] = 1.0f;
    }

    Color::Color(float r, float g, float b, float a = 1.0f)
    {
        colors_[0] = r;
        colors_[1] = g;
        colors_[2] = b;
        colors_[3] = a;
    }

    Color::~Color()
    {
    }

    float Color::getRed() const
    {
        return colors_[0];
    }

    float Color::getGreen() const
    {
        return colors_[1];
    }

    float Color::getBlue() const
    {
        return colors_[2];
    }

    float Color::getAlpha() const
    {
        return colors_[3];
    }

    bool Color::operator==(Color const& rhs) const noexcept
    {
        return colors_ == rhs.colors_;
    }

    bool Color::operator!=(Color const& rhs) const noexcept
    {
        return colors_ != rhs.colors_;
    }

    bool Color::operator<(Color const& rhs) const noexcept
    {
        return colors_ < rhs.colors_;
    }

    bool Color::operator>(Color const& rhs) const noexcept
    {
        return colors_ > rhs.colors_;
    }

    std::array<float, 4> const& Color::getArray() const
    {
        return colors_;
    }

    inline Color operator*(Color const& c, float scalar)
    {
        return Color(
                c.getRed() * scalar,
                c.getGreen() * scalar,
                c.getBlue() * scalar,
                c.getAlpha()
                );
    }

    inline Color operator*(float scalar, Color const& c)
    {
        return c * scalar;
    }
}

namespace btl
{
    template <> struct is_contiguously_hashable<avg::Color>
        : std::true_type {};
}

