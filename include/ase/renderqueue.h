#pragma once

#include "asevisibility.h"

#include <memory>

namespace ase
{
    class CommandBuffer;
    class RenderQueueImpl;

    class ASE_EXPORT RenderQueue
    {
    public:
        explicit RenderQueue(std::shared_ptr<RenderQueueImpl> impl);
        RenderQueue(RenderQueue const&) = default;
        RenderQueue(RenderQueue&&) = default;

        RenderQueue& operator=(RenderQueue const&) = default;
        RenderQueue& operator=(RenderQueue&&) = default;

        void flush();
        void finish();
        void submit(CommandBuffer&& commandBuffer);

        template <class T>
        T& getImpl()
        {
            return reinterpret_cast<T&>(*d());
        }

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    private:
        std::shared_ptr<RenderQueueImpl> deferred_;
        inline RenderQueueImpl* d() { return deferred_.get(); }
        inline RenderQueueImpl const* d() const { return deferred_.get(); }
    };
} // namespace ase

