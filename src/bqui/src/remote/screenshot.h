#pragma once

#include "dispatcher.h"

#include "bqui/remote/remotedriver.h"
#include "bqui/widget/introspection.h"
#include "bqui/bquivisibility.h"

#include <ase/readback.h>
#include <ase/vector.h>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace bqui::remote
{
    inline constexpr int kCaptureUnsupported = -32010;
    inline constexpr int kCaptureBusy = -32011;

    /**
     * @brief How many screenshots a session may have unanswered at once.
     */
    inline constexpr size_t kMaxScreenshotsInFlight = 4;

    /**
     * @brief A rectangle in window-space logical units: `x`, `y` is its
     * bottom-left corner and y grows upwards, as in introspection obbs.
     */
    struct LogicalRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    /**
     * @brief Which pixels of a captured frame a screenshot takes and how
     * large an image it makes of them.
     *
     * `origin` is the top-left pixel taken, counting rows from the top as
     * `ase::ReadbackImage` does. `captured` is `origin` and `size` in logical
     * units: the requested region grown to whole pixels.
     */
    struct ScreenshotPlan
    {
        ase::Vector2i origin = ase::Vector2i(0, 0);
        ase::Vector2i size = ase::Vector2i(0, 0);
        ase::Vector2i outputSize = ase::Vector2i(0, 0);
        LogicalRect captured;
    };

    /**
     * @brief Plan a screenshot of `region`, or of the whole frame without
     * one, from a frame of `frameSize` pixels at `scalingFactor` pixels per
     * logical unit, scaled by `scale`.
     *
     * The region is clamped to the frame; nothing is planned when it is
     * empty after clamping.
     */
    BQUI_EXPORT std::optional<ScreenshotPlan> planScreenshot(
            ase::Vector2i frameSize, float scalingFactor,
            std::optional<LogicalRect> region, float scale);

    /**
     * @brief The pixels `plan` describes, as tightly packed 8-bit RGB rows,
     * top row first.
     *
     * Alpha is dropped. A smaller output averages each output pixel's area
     * of the source in linear light.
     */
    BQUI_EXPORT std::vector<std::uint8_t> renderScreenshot(
            ase::ReadbackImage const& image, ScreenshotPlan const& plan);

    /**
     * @brief What a `window.screenshot` request asks for.
     */
    struct ScreenshotRequest
    {
        uint64_t window = 0;
        std::optional<LogicalRect> region;
        std::optional<uint64_t> widget;
        float scale = 1.0f;
    };

    /**
     * @brief Parse the params of a `window.screenshot` of `window`.
     *
     * Invalid params are thrown as `RpcError`.
     */
    ScreenshotRequest parseScreenshot(nlohmann::json const& params,
            uint64_t window);

    /**
     * @brief The bounds of the widget `id` in a resolved introspection tree.
     */
    std::optional<LogicalRect> widgetBounds(widget::Introspection const& node,
            uint64_t id);

    /**
     * @brief What a screenshot's plan takes from the frame it captures.
     */
    struct FrameGeometry
    {
        float scalingFactor = 1.0f;
        std::optional<LogicalRect> region;
        bool widgetMissing = false;
    };

    /**
     * @brief The geometry of `request` in `window`'s current frame.
     */
    FrameGeometry readGeometry(RemoteWindow const& window,
            ScreenshotRequest const& request);

    nlohmann::json toJson(LogicalRect const& rect);

    /**
     * @brief The error a screenshot answers with when its capture failed.
     */
    RpcError captureError(ase::ReadbackError error, uint64_t window);
} // namespace bqui::remote
