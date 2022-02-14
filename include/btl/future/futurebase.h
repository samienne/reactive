#pragma once

#include <btl/moveonlyfunction.h>
#include <btl/spinlock.h>

#include <btl/tsan.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>

namespace btl::future
{
    template <typename T>
    using FutureType = decltype(std::declval<T>().get());

    class FutureBase : public std::enable_shared_from_this<FutureBase>
    {
    public:
        virtual ~FutureBase() = default;
    };
} // namespace btl::future

