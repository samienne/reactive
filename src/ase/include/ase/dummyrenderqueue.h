#pragma once

#include "renderqueueimpl.h"

#include <memory>

namespace ase
{
    class FenceDrain;

    class DummyRenderQueue : public RenderQueueImpl
    {
    public:
        /**
         * @brief Headless render queue: nothing draws and fences signal
         * on submit().
         */
        DummyRenderQueue();

        // From RenderQueueImpl
        void flush() override;
        void finish() override;
        void submit(CommandBuffer&& renderQueue) override;

    private:
        std::shared_ptr<FenceDrain> drain_;
    };
} // namespace ase
