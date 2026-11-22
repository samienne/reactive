#pragma once

#include "fenceimpl.h"

#include "asevisibility.h"

#include <btl/future/promise.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace ase
{
    /**
     * @brief A fence its backend marks signaled from whichever thread
     * observes the GPU work complete.
     *
     * `signal()` does not fulfil the waiting futures itself: it hands their
     * promises back for a `FenceDrain` to fulfil off that thread.
     */
    class ASE_EXPORT BasicFence : public FenceImpl
    {
    public:
        bool isSignaled() const override;
        btl::future::Future<> whenSignaled() override;

        /**
         * @brief Mark the fence signaled and move the promises of the
         * futures still waiting on it onto `signaled`.
         */
        void signal(std::vector<btl::future::Promise<>>& signaled);

    private:
        std::mutex mutex_;
        std::atomic<bool> signaled_ = false;
        std::vector<btl::future::Promise<>> promises_;
    };

    /**
     * @brief Fulfils signaled fences' promises in order on the btl thread
     * pool.
     *
     * Promises posted while it runs are fulfilled after the earlier ones.
     * Must be owned by a `std::shared_ptr`.
     */
    class ASE_EXPORT FenceDrain :
        public std::enable_shared_from_this<FenceDrain>
    {
    public:
        void post(std::vector<btl::future::Promise<>> promises);

    private:
        void drain();

        std::mutex mutex_;
        std::vector<btl::future::Promise<>> pending_;
        bool scheduled_ = false;
    };
} // namespace ase
