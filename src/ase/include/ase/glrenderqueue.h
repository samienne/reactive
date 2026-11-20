#pragma once

#include "glrenderstate.h"
#include "renderqueueimpl.h"

#include "asevisibility.h"

#include <memory>

namespace ase
{
    class GlDispatchedContext;

    class ASE_EXPORT GlRenderQueue : public RenderQueueImpl
    {
    public:
        GlRenderQueue(std::shared_ptr<GlDispatchedContext> context);
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
        GlRenderState renderState_;
    };
} // namespace ase

