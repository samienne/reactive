#include "capture.h"

#include <utility>

namespace bqui::remote
{

void captureOnLoop(RemoteWindow& window, btl::RunLoop& loop,
        std::weak_ptr<void> alive, LoopCaptureCallback done)
{
    window.captureNextFrame(
            [&loop, alive = std::move(alive), done = std::move(done)](
                ase::CaptureResult result) mutable
            {
                // libc++ leaves a moved-from std::function holding a copy of
                // a small target, which would die with this callback, off the
                // loop thread.
                loop.post([alive = std::move(alive),
                        done = std::exchange(done, nullptr),
                        result = std::move(result)](
                            btl::RunLoop::Controller& controller) mutable
                    {
                        if (alive.expired())
                            return;

                        done(controller, std::move(result));
                    });
            });
}

} // namespace bqui::remote
