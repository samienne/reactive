#pragma once

#include <avg/font.h>
#include <avg/color.h>

#include <btl/shared.h>
#include <btl/visibility.h>

#include <ostream>

namespace reactive::widget
{
    struct ThemeDeferred;

    class BTL_VISIBLE Theme
    {
    public:
        Theme();

        avg::Color const& getEmphasized() const;
        avg::Color const& getPrimary() const;
        avg::Color const& getSecondary() const;
        avg::Color const& getBackgroundHighlight() const;
        avg::Color const& getBackground() const;
        avg::Color const& getYellow() const;
        avg::Color const& getOrange() const;
        avg::Color const& getRed() const;
        avg::Color const& getMagenta() const;
        avg::Color const& getBrmagenta() const;
        avg::Color const& getBlue() const;
        avg::Color const& getCyan() const;
        avg::Color const& getGreen() const;
        avg::Font const& getFont() const;
        float getTextHeight() const;

        bool operator==(Theme const& rhs) const;
        bool operator!=(Theme const& rhs) const;

    private:
        btl::shared<ThemeDeferred> deferred_;
    };

    inline std::ostream& operator<<(std::ostream& stream, Theme const&)
    {
        return stream << "Theme()" << std::endl;
    }
} // namespace reactice::widget

