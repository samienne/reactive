#include "screenshot.h"

#include <avg/rect.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace bqui::remote
{

using nlohmann::json;

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
            {
                taps[static_cast<size_t>(i)].push_back(
                        { j, static_cast<float>(covered / ratio) });
            }
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

std::optional<float> finiteFloat(json const& value)
{
    if (!value.is_number())
        return std::nullopt;

    double const number = value.get<double>();
    if (!std::isfinite(number)
            || std::abs(number) > (std::numeric_limits<float>::max)())
    {
        return std::nullopt;
    }

    return static_cast<float>(number);
}

LogicalRect parseRegion(json const& region)
{
    if (!region.is_object())
    {
        throw RpcError{ kInvalidParams,
            "'region' must be an object with x, y, width and height" };
    }

    auto field = [&](char const* key)
    {
        auto it = region.find(key);
        auto value = it == region.end() ? std::nullopt : finiteFloat(*it);
        if (!value)
        {
            throw RpcError{ kInvalidParams,
                std::string("'region.") + key + "' must be a finite number" };
        }
        return *value;
    };

    LogicalRect rect{ field("x"), field("y"), field("width"),
        field("height") };
    if (!(rect.width > 0.0f) || !(rect.height > 0.0f))
    {
        throw RpcError{ kInvalidParams,
            "'region' must have a positive width and height" };
    }

    return rect;
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
    auto scaled = [&](int pixels)
    {
        return (std::max)(1, static_cast<int>(std::lround(pixels * scale)));
    };
    plan.outputSize = ase::Vector2i(scaled(plan.size.x()),
            scaled(plan.size.y()));
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
    {
        throw std::invalid_argument("screenshot: the plan does not fit the "
                "image");
    }

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
        {
            for (size_t x = 0; x < inWidth; ++x)
            {
                std::copy_n(source(x, y), 3,
                        out.data() + (y * outWidth + x) * 3);
            }
        }
        return out;
    }

    auto const& decode = srgbToLinear();
    auto const columns = areaTaps(plan.size.x(), plan.outputSize.x());
    auto const rows = areaTaps(plan.size.y(), plan.outputSize.y());

    std::vector<float> narrowed(inHeight * outWidth * 3, 0.0f);
    for (size_t y = 0; y < inHeight; ++y)
    {
        for (size_t x = 0; x < outWidth; ++x)
        {
            for (auto const& tap : columns[x])
            {
                auto const* pixel = source(static_cast<size_t>(tap.index), y);
                float* dest = narrowed.data() + (y * outWidth + x) * 3;
                for (size_t c = 0; c < 3; ++c)
                    dest[c] += tap.weight * decode[pixel[c]];
            }
        }
    }

    for (size_t y = 0; y < outHeight; ++y)
    {
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
    }

    return out;
}

ScreenshotRequest parseScreenshot(json const& params, uint64_t window)
{
    ScreenshotRequest request;
    request.window = window;

    auto region = params.find("region");
    auto widget = params.find("widget");
    if (region != params.end() && widget != params.end())
    {
        throw RpcError{ kInvalidParams,
            "give 'region' or 'widget', not both" };
    }

    if (region != params.end())
        request.region = parseRegion(*region);

    if (widget != params.end())
    {
        if (!widget->is_number_unsigned())
        {
            throw RpcError{ kInvalidParams,
                "'widget' must be an introspection id" };
        }
        request.widget = widget->get<uint64_t>();
    }

    auto scale = params.find("scale");
    if (scale != params.end())
    {
        auto value = finiteFloat(*scale);
        if (!value || !(*value > 0.0f) || *value > 1.0f)
            throw RpcError{ kInvalidParams, "'scale' must be in (0, 1]" };
        request.scale = *value;
    }

    return request;
}

std::optional<LogicalRect> widgetBounds(widget::Introspection const& node,
        uint64_t id)
{
    if (node.id && node.id->getValue() == id)
    {
        auto rect = node.obb.getBoundingRect();
        return LogicalRect{ rect.getLeft(), rect.getBottom(), rect.getWidth(),
            rect.getHeight() };
    }

    for (auto const& child : node.children)
    {
        if (!child)
            continue;

        if (auto bounds = widgetBounds(*child, id))
            return bounds;
    }

    return std::nullopt;
}

FrameGeometry readGeometry(RemoteWindow const& window,
        ScreenshotRequest const& request)
{
    FrameGeometry geometry;
    geometry.scalingFactor = window.scalingFactor();
    geometry.region = request.region;
    if (request.widget)
    {
        geometry.region = widgetBounds(window.introspect(), *request.widget);
        geometry.widgetMissing = !geometry.region;
    }

    return geometry;
}

json toJson(LogicalRect const& rect)
{
    return {
        { "x", rect.x },
        { "y", rect.y },
        { "width", rect.width },
        { "height", rect.height },
    };
}

RpcError captureError(ase::ReadbackError error, uint64_t window)
{
    json data = { { "window", window } };
    std::string const name = "window " + std::to_string(window);

    switch (error)
    {
    case ase::ReadbackError::Unsupported:
        data["reason"] = "unsupported";
        return { kCaptureUnsupported,
            name + " cannot capture: its frame cannot be read back",
            data };
    case ase::ReadbackError::Failed:
        break;
    }

    data["reason"] = "failed";
    return { kInternalError, name + " failed to read its frame back", data };
}

} // namespace bqui::remote
