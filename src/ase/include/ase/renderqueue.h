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

        /**
         * @brief Hand the recorded commands to the backend, to execute in
         * order.
         *
         * They may run now, on the calling thread, or later; assume
         * neither. Everything a submitted buffer references - buffers,
         * textures, framebuffers, uniform ranges - stays alive in the
         * backend until those commands have completed, so the caller may
         * drop its handles right after submit. Completion is observed
         * through a fence pushed into the buffer.
         */
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

