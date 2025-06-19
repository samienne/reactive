#pragma once

#include "future.h"
#include "futurebase.h"

#include <atomic>
#include <vector>

namespace btl::future
{
    template <typename T>
    class MergedFuture final :
        public FutureControl<std::vector<T>>
    {
    public:
        MergedFuture(std::vector<Future<T>> futures) :
            count_(static_cast<int>(futures.size())),
            futures_(std::move(futures))
        {
        }

        void init()
        {
            std::weak_ptr<FutureControl<std::vector<T>>> control =
                std::static_pointer_cast<FutureControl<std::vector<T>>>(
                        this->shared_from_this());

            btl::forEach(futures_, [this, &control](auto&& future) mutable
            {
                if (failed_.load(std::memory_order_acquire))
                    return;

                future.addCallback_([control](auto& valueControl) mutable
                    {
                        if (auto p = control.lock())
                        {
                            auto* ptr = static_cast<MergedFuture<T>*>(
                                p.get());

                            if (valueControl.hasValue())
                                ptr->reportFutureReady();
                            else
                                ptr->reportFailure(valueControl.getException());
                        }
                    });

            });

            if (failed_.load(std::memory_order_acquire))
                futures_.clear();

            initialized_.store(true, std::memory_order_release);
        }

        void reportFailure(std::exception_ptr err)
        {
            bool expected = false;
            bool hadNotFailed = failed_.compare_exchange_strong(expected, true);
            if (hadNotFailed)
            {
                this->setFailure(std::move(err));

                if (initialized_.load(std::memory_order_acquire))
                    futures_.clear();
            }
        }

        void reportFutureReady()
        {
            auto count = count_.fetch_sub(1, std::memory_order_acquire);

            assert(count >= 1);

            if (count == 1)
            {
                std::vector<T> result;
                result.reserve(futures_.size());
                for (auto&& future : futures_)
                    result.push_back(std::move(future).get());
                this->setValue(std::move(result));

                futures_.clear();
            }
        }

    private:
        std::atomic_int count_;
        std::atomic_bool failed_ = false;
        std::atomic_bool initialized_ = false;
        std::vector<Future<T>> futures_;
    };

    template <typename T>
    auto merge(std::vector<Future<T>> futures) -> Future<std::vector<T>>
    {
        auto control = std::make_shared<MergedFuture<T>>(
                std::move(futures));
        control->init();
        return Future<std::vector<T>>(std::move(control));
    }
} // btl::future

