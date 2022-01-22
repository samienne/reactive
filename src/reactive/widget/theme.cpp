#include "widget/theme.h"

namespace reactive
{
namespace widget
{

namespace color
{

float sRGBtoLinear(float sRGB)
{
    if (sRGB <= 0.04045)
        return sRGB / 12.92;
    else
        return pow((sRGB + 0.055) / 1.055, 2.4);
}

auto fromSrgb(float red, float green, float blue,
        float alpha = 1.0f) -> avg::Color
{
    return avg::Color(sRGBtoLinear(red), sRGBtoLinear(green),
            sRGBtoLinear(blue), alpha);
}

avg::Color const base03    = fromSrgb(0.0f, 43.0f/255.0f, 54.0f/255.0f);
avg::Color const base02    = fromSrgb(7.0f/255.0f, 54.0f/255.0f, 66.0f/255.0f);
avg::Color const base01    = fromSrgb(88.0f/255.0f, 110.0f/255.0f, 117.0f/255.0f);
avg::Color const base00    = fromSrgb(101.0f/255.0f, 123.0f/255.0f, 131.0f/255.0f);
avg::Color const base0     = fromSrgb(131.0f/255.0f, 148.0f/255.0f, 150.0f/255.0f);
avg::Color const base1     = fromSrgb(147.0f/255.0f, 161.0f/255.0f, 161.0f/255.0f);
avg::Color const base2     = fromSrgb(238.0f/255.0f, 232.0f/255.0f, 213.0f/255.0f);
avg::Color const base3     = fromSrgb(253.0f/255.0f, 246.0f/255.0f, 227.0f/255.0f);
avg::Color const yellow    = fromSrgb(181.0f/255.0f, 137.0f/255.0f, 0.0f/255.0f);
avg::Color const orange    = fromSrgb(203.0f/255.0f, 75.0f/255.0f, 22.0f/255.0f);
avg::Color const red       = fromSrgb(220.0f/255.0f, 50.0f/255.0f, 47.0f/255.0f);
avg::Color const magenta   = fromSrgb(211.0f/255.0f, 54.0f/255.0f, 130.0f/255.0f);
avg::Color const brmagenta = fromSrgb(108.0f/255.0f, 113.0f/255.0f, 196.0f/255.0f);
avg::Color const blue      = fromSrgb(38.0f/255.0f, 139.0f/255.0f, 210.0f/255.0f);
avg::Color const cyan      = fromSrgb(42.0f/255.0f, 161.0f/255.0f, 152.0f/255.0f);
avg::Color const green     = fromSrgb(133.0f/255.0f, 153.0f/255.0f, 0.0f/255.0f);
} // namespace color


struct ThemeDeferred
{
    ThemeDeferred();

    avg::Color emphasized           = color::base1;
    avg::Color primary              = color::base0;
    avg::Color secondary            = color::base01;
    avg::Color backgroundHighlight  = color::base02;
    avg::Color background           = color::base03;
    avg::Color yellow               = color::yellow;
    avg::Color orange               = color::orange;
    avg::Color red                  = color::red;
    avg::Color magenta              = color::magenta;
    avg::Color brmagenta            = color::brmagenta;
    avg::Color blue                 = color::blue;
    avg::Color cyan                 = color::cyan;
    avg::Color green                = color::green;
    avg::Font font = avg::Font("../../data/fonts/OpenSans-Regular.ttf", 0);
    float textHeight = 12.0f;

    bool operator==(ThemeDeferred const& rhs) const
    {
        if (emphasized != rhs.emphasized)
            return false;
        if (primary != rhs.primary)
            return false;
        if (secondary != rhs.secondary)
            return false;
        if (backgroundHighlight != rhs.backgroundHighlight)
            return false;
        if (background != rhs.background)
            return false;
        if (yellow != rhs.yellow)
            return false;
        if (orange != rhs.orange)
            return false;
        if (red != rhs.red)
            return false;
        if (magenta != rhs.magenta)
            return false;
        if (brmagenta != rhs.brmagenta)
            return false;
        if (blue != rhs.blue)
            return false;
        if (cyan != rhs.cyan)
            return false;
        if (green != rhs.green)
            return false;
        if (font != rhs.font)
            return false;
        if (textHeight != rhs.textHeight)
            return false;

        return true;
    }
};

ThemeDeferred::ThemeDeferred() :
    emphasized          ( color::base1),
    primary             ( color::base0),
    secondary           ( color::base01),
    backgroundHighlight ( color::base02),
    background          ( color::base03),
    yellow(color::yellow),
    orange(color::orange),
    red(color::red),
    magenta(color::magenta),
    brmagenta(color::brmagenta),
    blue(color::blue),
    cyan(color::cyan),
    green(color::green),
    font(avg::Font("../../data/fonts/OpenSans-Regular.ttf", 0)),
    textHeight(12.0f)
{
}

Theme::Theme() :
    deferred_(std::make_shared<ThemeDeferred>())
{
}

avg::Color const& Theme::getEmphasized() const
{
    return deferred_->emphasized;
}

avg::Color const& Theme::getPrimary() const
{
    return deferred_->primary;
}

avg::Color const& Theme::getSecondary() const
{
    return deferred_->secondary;
}

avg::Color const& Theme::getBackgroundHighlight() const
{
    return deferred_->backgroundHighlight;
}

avg::Color const& Theme::getBackground() const
{
    return deferred_->background;
}

avg::Color const& Theme::getYellow() const
{
    return deferred_->yellow;
}

avg::Color const& Theme::getOrange() const
{
    return deferred_->orange;
}

avg::Color const& Theme::getRed() const
{
    return deferred_->red;
}

avg::Color const& Theme::getMagenta() const
{
    return deferred_->magenta;
}

avg::Color const& Theme::getBrmagenta() const
{
    return deferred_->brmagenta;
}

avg::Color const& Theme::getBlue() const
{
    return deferred_->blue;
}

avg::Color const& Theme::getCyan() const
{
    return deferred_->cyan;
}

avg::Color const& Theme::getGreen() const
{
    return deferred_->green;
}

avg::Font const& Theme::getFont() const
{
    return deferred_->font;
}

float Theme::getTextHeight() const
{
    return deferred_->textHeight;
}

void Theme::setSecondary(avg::Color const& color)
{
    deferred_->secondary = color;
}

bool Theme::operator==(Theme const& rhs) const
{
    if (deferred_ == rhs.deferred_)
        return true;

    return *deferred_ == *rhs.deferred_;
}

bool Theme::operator!=(Theme const& rhs) const
{
    if (deferred_ == rhs.deferred_)
        return false;

    return !(*deferred_ == *rhs.deferred_);
}

} } // namespace

