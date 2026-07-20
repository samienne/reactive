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
        // count_ starts one above the input count; init() owns the extra count
        // and releases it only after it has stopped walking futures_, so
        // completion never destroys futures_ from under that walk.
        MergedFuture(std::vector<Future<T>> futures) :
            count_(static_cast<int>(futures.size()) + 1),
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

            arriveAndClear();

            reportFutureReady();
        }

        void reportFailure(std::exception_ptr err)
        {
            bool expected = false;
            bool hadNotFailed = failed_.compare_exchange_strong(expected, true);
            if (hadNotFailed)
            {
                arriveAndClear();

                this->setFailure(std::move(err));
            }
        }

        void reportFutureReady()
        {
            auto count = count_.fetch_sub(1, std::memory_order_acq_rel);

            assert(count >= 1);

            if (count == 1)
            {
                std::vector<T> result;
                result.reserve(futures_.size());
                for (auto&& future : futures_)
                    result.push_back(std::move(future).get());

                arriveAndClear();

                this->setValue(std::move(result));
            }
        }

    private:
        // Arrivals come from init(), the success path and the failure path,
        // and exactly two of them ever happen: a failed input reports failure
        // instead of readiness, so count_ never reaches one. init() arrives
        // once its walk is over, so the second arrival, the one that clears,
        // never runs mid-walk. Both callers arrive before publishing the
        // result, so a ready merged future has already dropped its inputs.
        void arriveAndClear()
        {
            if (clearArrivals_.fetch_add(1, std::memory_order_acq_rel) == 1)
                futures_.clear();
        }

        std::atomic_int count_;
        std::atomic_bool failed_ = false;
        std::atomic_int clearArrivals_ = 0;
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

