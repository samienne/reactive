#pragma once

#include "fence.h"
#include "renderqueueimpl.h"

#include <memory>
#include <vector>

namespace ase
{
    class FenceDrain;

    class DummyRenderQueue : public RenderQueueImpl
    {
    public:
        /**
         * @brief Headless render queue: nothing draws and fences signal
         * on submit(), unless they are being held.
         */
        DummyRenderQueue();

        // From RenderQueueImpl
        void flush() override;
        void finish() override;
        void submit(CommandBuffer&& renderQueue) override;

        void holdFences();
        void releaseFences();

    private:
        std::shared_ptr<FenceDrain> drain_;
        bool holding_ = false;
        std::vector<Fence> held_;
    };
} // namespace ase
