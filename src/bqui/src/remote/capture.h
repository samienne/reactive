#pragma once

#include "bqui/remote/remotedriver.h"

#include "bqui/bquivisibility.h"

#include <ase/readback.h>

#include <btl/runloop.h>

#include <functional>
#include <memory>

namespace bqui::remote
{
    /**
     * @brief Receives a capture's result on the loop thread.
     */
    using LoopCaptureCallback = std::function<void(btl::RunLoop::Controller&,
            ase::CaptureResult)>;

    /**
     * @brief Capture `window`'s next frame and hand the result to `done` on
     * `loop`'s thread.
     *
     * `done` runs in a loop callback of its own, never inside this call, and
     * not at all once `alive` has expired by then. `loop` must outlive the
     * window's pending capture.
     *
     * Once the window completes the capture, only the posted task owns `done`,
     * so `done` is destroyed on `loop`'s thread; if the loop never runs the
     * task, it is destroyed with the loop instead.
     */
    BQUI_EXPORT void captureOnLoop(RemoteWindow& window, btl::RunLoop& loop,
            std::weak_ptr<void> alive, LoopCaptureCallback done);
} // namespace bqui::remote
