#pragma once

#include "future.h"
#include "futurebase.h"

#include <vector>

namespace btl
{
    namespace future
    {
        template <typename T>
        class MergedFuture final :
            public FutureControl<std::vector<T>>
        {
        public:
            MergedFuture(std::vector<Future<T>> futures) :
                count_(futures.size()),
                futures_(std::move(futures))
            {
            }

            void init()
            {
                std::weak_ptr<FutureControl<std::vector<T>>> control =
                    std::static_pointer_cast<FutureControl<std::vector<T>>>(
                            this->shared_from_this());

                btl::forEach(futures_, [&control](auto&& future) mutable
                {
                    future.addCallback_([control](auto&) mutable
                        {
                            if (auto p = control.lock())
                            {
                                auto* ptr = static_cast<MergedFuture<T>*>(
                                    p.get());

                                ptr->reportFutureReady();
                            }
                        });

                });
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
                }
            }

        private:
            std::atomic_int count_;
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
    } // future
} // btl

