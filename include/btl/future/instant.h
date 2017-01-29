#pragma once

#include <future>

namespace btl
{
    namespace future
    {
        template <typename T>
        auto instant(T&& t) -> std::future<typename std::decay<T>::type>
        {
            using value_type = typename std::decay<T>::type;
            std::promise<value_type> promise;
            auto future = promise.get_future();
            promise.set(std::forward<T>(t));

            return std::move(future);
        }
    } // future
} // btl

