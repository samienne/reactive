#include "capture.h"

#include <exception>
#include <memory>
#include <utility>

namespace bqui::remote
{

namespace
{
    struct PendingReadback
    {
        void complete(LoopReadbackResult result)
        {
            if (!done || alive.expired())
                return;

            // libc++ leaves a moved-from std::function holding a copy of a
            // small target, which would die with this object, off the loop
            // thread.
            loop.post([alive = std::move(alive),
                    done = std::exchange(done, nullptr),
                    result = std::move(result)](
                        btl::RunLoop::Controller& controller) mutable
                {
                    if (alive.expired())
                        return;

                    done(controller, std::move(result));
                });
        }

        btl::RunLoop& loop;
        std::weak_ptr<void> alive;
        LoopReadbackCallback done;
    };

    ase::ReadbackError readbackError(std::exception_ptr const& err)
    {
        try
        {
            std::rethrow_exception(err);
        }
        catch (ase::ReadbackFailure const& failure)
        {
            return failure.error();
        }
        catch (...)
        {
            return ase::ReadbackError::Failed;
        }
    }
} // anonymous namespace

void readbackOnLoop(RemoteWindow& window, btl::RunLoop& loop,
        std::weak_ptr<void> alive, LoopReadbackCallback done)
{
    auto pending = std::make_shared<PendingReadback>(PendingReadback{
            loop, std::move(alive), std::move(done) });

    window.readback()
        .then([pending](ase::ReadbackImage image)
            {
                pending->complete(std::move(image));
            })
        .onFailure([pending](std::exception_ptr const& err)
            {
                pending->complete(readbackError(err));
            })
        .detach();
}

} // namespace bqui::remote
