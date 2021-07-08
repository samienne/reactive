#pragma once

#include "glrenderstate.h"
#include "renderqueueimpl.h"

#include <memory>

namespace ase
{
    class GlDispatchedContext;

    class GlRenderQueue : public RenderQueueImpl
    {
    public:
        GlRenderQueue(std::shared_ptr<GlDispatchedContext> context,
                std::function<void(Dispatched, Window&)> presentCallback
                );
        ~GlRenderQueue();

        void dispatch(std::function<void(GlFunctions const&)> f);

        GlDispatchedContext& getDispatcher();
        GlDispatchedContext const& getDispatcher() const;

        // From RenderQueueImpl
        void flush() override;
        void finish() override;
        void submit(CommandBuffer&& renderQueue) override;

    private:
        std::shared_ptr<GlDispatchedContext> dispatcher_;
        std::shared_ptr<GlDispatchedContext> context_;
        GlRenderState renderState_;
    };
} // namespace ase

