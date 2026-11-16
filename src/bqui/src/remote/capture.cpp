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
                loop.post([alive = std::move(alive), done = std::move(done),
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
