#pragma once

#include "renderqueueimpl.h"

namespace ase
{
    class DummyRenderQueue : public RenderQueueImpl
    {
    public:
        /** @brief Headless render queue: nothing draws and fences complete
         * inline on submit(). */
        DummyRenderQueue() = default;

        // From RenderQueueImpl
        void flush() override;
        void finish() override;
        void submit(CommandBuffer&& renderQueue) override;
    };
} // namespace ase
