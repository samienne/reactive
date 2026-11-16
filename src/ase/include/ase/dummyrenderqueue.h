#pragma once

#include "renderqueueimpl.h"

#include <btl/future/promise.h>

#include <vector>

namespace ase
{
    class DummyRenderQueue : public RenderQueueImpl
    {
    public:
        /** @brief Headless render queue: nothing draws and fences complete
         * inline on submit(), unless they are being held. */
        DummyRenderQueue() = default;

        // From RenderQueueImpl
        void flush() override;
        void finish() override;
        void submit(CommandBuffer&& renderQueue) override;

        void holdFences();
        void releaseFences();

    private:
        bool holding_ = false;
        std::vector<btl::future::Promise<>> held_;
    };
} // namespace ase
