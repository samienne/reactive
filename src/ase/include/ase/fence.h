#pragma once

#include "asevisibility.h"

#include <btl/future/future.h>

#include <memory>

namespace ase
{
    class FenceImpl;

    /**
     * @brief A point in a render queue that signals once the GPU has
     * finished the work submitted before it.
     *
     * Made by `RenderContext::makeFence()` and pushed with
     * `CommandBuffer::pushFence()`. A fence that is never submitted, or whose
     * queue is torn down before it runs, never signals; once it is destroyed
     * its pending futures fail with `btl::future::BrokenPromise`.
     */
    class ASE_EXPORT Fence
    {
    public:
        explicit Fence(std::shared_ptr<FenceImpl> impl);

        /**
         * @brief Whether the fence has signaled; cheap and callable from any
         * thread.
         */
        bool isSignaled() const;

        /**
         * @brief A future that completes once the fence signals.
         *
         * Ready at once if it already has, even while futures taken before
         * the signal are still being fulfilled, so its continuations can run
         * before theirs and on the calling thread. Otherwise its
         * continuations run on a btl thread pool thread, never on the
         * backend's render thread.
         */
        btl::future::Future<> whenSignaled();

        /**
         * @brief Casts the internal implementation to requested type.
         *
         * This is for internal use only and expects that you know the
         * actual type of implementation.
         */
        template <class T>
        T& getImpl()
        {
            return static_cast<T&>(*d());
        }

    private:
        inline FenceImpl* d() { return deferred_.get(); }
        inline FenceImpl const* d() const { return deferred_.get(); }

        std::shared_ptr<FenceImpl> deferred_;
    };
} // namespace ase
