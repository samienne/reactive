#pragma once

#include "apply.h"
#include "fmap.h"
#include "mbind.h"

#include <tuple>

namespace btl
{
    template <typename T, typename... Ts>
    class Bundle;

    template <typename... Ts>
    Bundle<Ts...> bundle(Ts&&... ts);

    template <typename T, typename... Ts>
    class Bundle
    {
    public:
        Bundle(T&& t, Ts&&... ts) :
            tuple_(std::forward<T>(t), std::forward<Ts>(ts)...)
        {
        }

        template <typename TFunc>
        auto fmap(TFunc&& func) &&
        -> Bundle<decltype(
                btl::fmap(
                    std::declval<TFunc>(),
                    std::declval<T>(),
                    std::declval<Ts>()...
                    )
                )
            >
        {
            return bundle(
                    btl::apply([this, &func](auto&&... us)
                        {
                            return btl::fmap(
                                std::forward<TFunc>(func),
                                std::forward<decltype(us)>(us)...
                                );
                        },
                        std::move(tuple_)
                        )
                    );
        }

        template <typename TFunc>
        auto mbind(TFunc&& func) &&
        -> Bundle<decltype(
                btl::mbind(
                    std::declval<TFunc>(),
                    std::declval<T>(),
                    std::declval<Ts>()...
                    )
                )
            >
        {
            return bundle(
                    btl::apply([this, &func](auto&&... us)
                        {
                            return btl::mbind(
                                std::forward<TFunc>(func),
                                std::forward<decltype(us)>(us)...
                                );
                        },
                        std::move(tuple_)
                        )
                    );
        }

        T&& operator*() &&
        {
            return std::get<0>(std::move(tuple_));
        }

    private:
        std::tuple<std::decay_t<T>, std::decay_t<Ts>...> tuple_;
    };

    template <typename... Ts>
    Bundle<Ts...> bundle(Ts&&... ts)
    {
        return Bundle<Ts...>(std::forward<Ts>(ts)...);
    }

    template <typename... Ts>
    auto bundleMove(Ts&&... ts)
    -> decltype(bundle(std::move(ts)...))
    {
        return bundle(std::move(ts)...);
    }
} // btl

