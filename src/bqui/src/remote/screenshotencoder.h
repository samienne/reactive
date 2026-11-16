#pragma once

#include "screenshot.h"

#include <ase/readback.h>

#include <btl/runloop.h>

#include <functional>
#include <memory>
#include <string>

namespace bqui::remote
{
    /**
     * @brief Encodes screenshots as PNG on the btl thread pool and hands each
     * result back on a loop's thread.
     *
     * The destructor waits for the encodes that have started, since each
     * posts to the loop; one that starts later does nothing.
     */
    class ScreenshotEncoder
    {
    public:
        /**
         * @brief Receives the PNG bytes, or in `error` why there are none.
         */
        using Done = std::function<void(btl::RunLoop::Controller&,
                std::string png, std::string const& error)>;

        explicit ScreenshotEncoder(btl::RunLoop& loop);
        ScreenshotEncoder(ScreenshotEncoder const&) = delete;
        ScreenshotEncoder& operator=(ScreenshotEncoder const&) = delete;
        ~ScreenshotEncoder();

        /**
         * @brief Crop and scale `capture` by `plan`, encode it and post the
         * result to `done`.
         */
        void encode(ase::FrameCapture capture, ScreenshotPlan const& plan,
                Done done);

    private:
        struct Jobs;

        btl::RunLoop& loop_;
        std::shared_ptr<Jobs> jobs_;
    };
} // namespace bqui::remote
