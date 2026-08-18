#pragma once

#include "renderqueueimpl.h"

namespace ase
{
    class GlDispatchedContext;

    class DummyRenderQueue : public RenderQueueImpl
    {
    public:
        DummyRenderQueue();

        // From RenderQueueImpl
        void flush() override;
        void finish() override;
        void submit(CommandBuffer&& renderQueue) override;
    };
} // namespace ase

