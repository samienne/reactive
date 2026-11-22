#pragma once

#include "bqui/remote/remotedriver.h"

#include "bqui/bquivisibility.h"

#include <ase/readback.h>

#include <btl/result.h>
#include <btl/runloop.h>

#include <functional>
#include <memory>

namespace bqui::remote
{
    /**
     * @brief The pixels of a readback, or why there are none.
     *
     * A readback whose promise was broken, or that failed in any other way
     * than with an `ase::ReadbackFailure`, is `ase::ReadbackError::Failed`.
     */
    using LoopReadbackResult = btl::Result<ase::ReadbackImage,
          ase::ReadbackError>;

    /**
     * @brief Receives a readback's result on the loop thread.
     */
    using LoopReadbackCallback = std::function<void(btl::RunLoop::Controller&,
            LoopReadbackResult)>;

    /**
     * @brief Read back `window`'s last rendered frame and hand the result to
     * `done` on `loop`'s thread.
     *
     * `done` runs in a loop callback of its own, never inside this call, and
     * not at all once `alive` has expired by then. The readback is pending
     * until its render context runs or drops it, and completing it posts to
     * `loop` from the completing thread unless `alive` has expired, so `loop`
     * must outlive every pending readback. Expiring `alive` does not make
     * destroying `loop` safe while a readback may still complete.
     *
     * Once the readback completes, only the posted task owns `done`, so
     * `done` is destroyed on `loop`'s thread; if the loop never runs the
     * task, it is destroyed with the loop instead. If `alive` has already
     * expired, nothing is posted and `done` is destroyed where the readback
     * completed.
     */
    BQUI_EXPORT void readbackOnLoop(RemoteWindow& window, btl::RunLoop& loop,
            std::weak_ptr<void> alive, LoopReadbackCallback done);
} // namespace bqui::remote
