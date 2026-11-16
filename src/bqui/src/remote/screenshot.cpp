#include "screenshot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace bqui::remote
{

namespace
{

// Keeps a region edge that lands on a pixel boundary, give or take float
// rounding, from taking in the neighbouring pixel.
constexpr double kEdgeTolerance = 1e-3;

struct Tap
{
    int index;
    float weight;
};

// For each output pixel, the source pixels its area covers and how much of
// it each one covers.
std::vector<std::vector<Tap>> areaTaps(int in, int out)
{
    std::vector<std::vector<Tap>> taps(static_cast<size_t>(out));
    double const ratio = static_cast<double>(in) / out;

    for (int i = 0; i < out; ++i)
    {
        double const start = i * ratio;
        double const end = (i + 1) * ratio;
        int const last = (std::min)(in, static_cast<int>(std::ceil(end)));
        for (int j = static_cast<int>(std::floor(start)); j < last; ++j)
        {
            double const covered = (std::min)(end, j + 1.0)
                - (std::max)(start, static_cast<double>(j));
            if (covered > 0.0)
                taps[static_cast<size_t>(i)].push_back(
                        { j, static_cast<float>(covered / ratio) });
        }
    }

    return taps;
}

std::array<float, 256> const& srgbToLinear()
{
    static std::array<float, 256> const table = []
    {
        std::array<float, 256> result{};
        for (size_t i = 0; i < result.size(); ++i)
        {
            double const c = i / 255.0;
            result[i] = static_cast<float>(c <= 0.04045
                    ? c / 12.92
                    : std::pow((c + 0.055) / 1.055, 2.4));
        }
        return result;
    }();
    return table;
}

std::uint8_t linearToSrgb(float linear)
{
    double const c = std::clamp(static_cast<double>(linear), 0.0, 1.0);
    double const encoded = c <= 0.0031308
        ? c * 12.92
        : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
    return static_cast<std::uint8_t>(std::lround(encoded * 255.0));
}

} // namespace

std::optional<ScreenshotPlan> planScreenshot(ase::Vector2i frameSize,
        float scalingFactor, std::optional<LogicalRect> region, float scale)
{
    int const frameWidth = frameSize.x();
    int const frameHeight = frameSize.y();
    if (frameWidth <= 0 || frameHeight <= 0 || !(scalingFactor > 0.0f))
        return std::nullopt;

    double const factor = scalingFactor;
    double const width = frameWidth / factor;
    double const height = frameHeight / factor;

    LogicalRect const r = region.value_or(LogicalRect{ 0.0f, 0.0f,
            static_cast<float>(width), static_cast<float>(height) });

    double const left = (std::max)(0.0, static_cast<double>(r.x));
    double const bottom = (std::max)(0.0, static_cast<double>(r.y));
    double const right = (std::min)(width,
            static_cast<double>(r.x) + r.width);
    double const top = (std::min)(height,
            static_cast<double>(r.y) + r.height);
    if (!(right > left) || !(top > bottom))
        return std::nullopt;

    auto low = [&](double v, int limit)
    {
        int p = static_cast<int>(std::floor(v * factor + kEdgeTolerance));
        return std::clamp(p, 0, limit - 1);
    };
    auto high = [&](double v, int limit, int lowPixel)
    {
        int p = static_cast<int>(std::ceil(v * factor - kEdgeTolerance));
        return std::clamp(p, lowPixel + 1, limit);
    };

    int const x0 = low(left, frameWidth);
    int const x1 = high(right, frameWidth, x0);
    int const y0 = low(bottom, frameHeight);
    int const y1 = high(top, frameHeight, y0);

    ScreenshotPlan plan;
    plan.origin = ase::Vector2i(x0, frameHeight - y1);
    plan.size = ase::Vector2i(x1 - x0, y1 - y0);
    plan.outputSize = ase::Vector2i(
            (std::max)(1, static_cast<int>(std::lround(plan.size.x() * scale))),
            (std::max)(1, static_cast<int>(std::lround(plan.size.y() * scale))));
    plan.captured = LogicalRect{
        static_cast<float>(x0 / factor),
        static_cast<float>(y0 / factor),
        static_cast<float>((x1 - x0) / factor),
        static_cast<float>((y1 - y0) / factor),
    };

    return plan;
}

std::vector<std::uint8_t> renderScreenshot(ase::ReadbackImage const& image,
        ScreenshotPlan const& plan)
{
    size_t const imageWidth = static_cast<size_t>(
            (std::max)(0, static_cast<int>(image.size.x())));
    size_t const imageHeight = static_cast<size_t>(
            (std::max)(0, static_cast<int>(image.size.y())));
    if (image.pixels.size() != imageWidth * imageHeight * 4
            || plan.origin.x() < 0 || plan.origin.y() < 0
            || plan.size.x() <= 0 || plan.size.y() <= 0
            || plan.origin.x() + plan.size.x() > image.size.x()
            || plan.origin.y() + plan.size.y() > image.size.y()
            || plan.outputSize.x() <= 0 || plan.outputSize.y() <= 0)
        throw std::invalid_argument("screenshot: the plan does not fit the "
                "image");

    size_t const inWidth = static_cast<size_t>(plan.size.x());
    size_t const inHeight = static_cast<size_t>(plan.size.y());
    size_t const outWidth = static_cast<size_t>(plan.outputSize.x());
    size_t const outHeight = static_cast<size_t>(plan.outputSize.y());

    auto source = [&](size_t x, size_t y)
    {
        return image.pixels.data()
            + ((plan.origin.y() + y) * imageWidth + plan.origin.x() + x) * 4;
    };

    std::vector<std::uint8_t> out(outWidth * outHeight * 3);

    if (outWidth == inWidth && outHeight == inHeight)
    {
        for (size_t y = 0; y < inHeight; ++y)
            for (size_t x = 0; x < inWidth; ++x)
                std::copy_n(source(x, y), 3, out.data() + (y * outWidth + x) * 3);
        return out;
    }

    auto const& decode = srgbToLinear();
    auto const columns = areaTaps(plan.size.x(), plan.outputSize.x());
    auto const rows = areaTaps(plan.size.y(), plan.outputSize.y());

    std::vector<float> narrowed(inHeight * outWidth * 3, 0.0f);
    for (size_t y = 0; y < inHeight; ++y)
        for (size_t x = 0; x < outWidth; ++x)
            for (auto const& tap : columns[x])
            {
                auto const* pixel = source(static_cast<size_t>(tap.index), y);
                float* dest = narrowed.data() + (y * outWidth + x) * 3;
                for (size_t c = 0; c < 3; ++c)
                    dest[c] += tap.weight * decode[pixel[c]];
            }

    for (size_t y = 0; y < outHeight; ++y)
        for (size_t x = 0; x < outWidth; ++x)
        {
            float sum[3] = {};
            for (auto const& tap : rows[y])
            {
                float const* pixel = narrowed.data()
                    + (static_cast<size_t>(tap.index) * outWidth + x) * 3;
                for (size_t c = 0; c < 3; ++c)
                    sum[c] += tap.weight * pixel[c];
            }

            for (size_t c = 0; c < 3; ++c)
                out[(y * outWidth + x) * 3 + c] = linearToSrgb(sum[c]);
        }

    return out;
}

} // namespace bqui::remote
