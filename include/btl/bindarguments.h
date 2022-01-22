#pragma once

#include "cloneoncopy.h"

#include <functional>
#include <tuple>

namespace btl
{
    template <typename TFunc, typename... Ts>
    class ArgumentBinder
    {
        ArgumentBinder(TFunc func, Ts... ts) :
            func_(std::move(func)),
            ts_(std::make_tuple(std::move(ts)...))
        {
        }

        template <typename... Us>
        auto operator()(Us&&... us) const
        {
            return std::apply(
                [&](auto&&... ts)
                {
                    return std::invoke(
                            *func_,
                            std::forward<Us>(us)...,
                            btl::clone(ts)...
                            );
                },
                ts_
                );
        }

    private:
        CloneOnCopy<TFunc> func_;
        CloneOnCopy<std::tuple<Ts...>> ts_;
    };

    template <typename TFunc, typename T>
    class ArgumentBinder<TFunc, T>
    {
    public:
        ArgumentBinder(TFunc func, T t) :
            func_(std::move(func)),
            t_(std::move(t))
        {
        }

        template <typename... Us>
        auto operator()(Us&&... us) const
        {
            return std::invoke(
                    *func_,
                    std::forward<Us>(us)...,
                    btl::clone(*t_)
                    );
        }

    private:
        CloneOnCopy<TFunc> func_;
        CloneOnCopy<T> t_;
    };

    template <typename TFunc, typename T, typename U>
    class ArgumentBinder<TFunc, T, U>
    {
    public:
        ArgumentBinder(TFunc func, T t, U u) :
            func_(std::move(func)),
            t_(std::move(t)),
            u_(std::move(u))
        {
        }

        template <typename... Us>
        auto operator()(Us&&... us) const
        {
            return std::invoke(
                    *func_,
                    std::forward<Us>(us)...,
                    btl::clone(*t_),
                    btl::clone(*u_)
                    );
        }

    private:
        CloneOnCopy<TFunc> func_;
        CloneOnCopy<T> t_;
        CloneOnCopy<U> u_;
    };

    template <typename TFunc, typename T, typename U, typename V>
    class ArgumentBinder<TFunc, T, U, V>
    {
    public:
        ArgumentBinder(TFunc func, T t, U u, V v) :
            func_(std::move(func)),
            t_(std::move(t)),
            u_(std::move(u)),
            v_(std::move(v))
        {
        }

        template <typename... Us>
        auto operator()(Us&&... us) const
        {
            return std::invoke(
                    *func_,
                    std::forward<Us>(us)...,
                    clone(*t_),
                    clone(*u_),
                    clone(*v_)
                    );
        }

    private:
        CloneOnCopy<TFunc> func_;
        CloneOnCopy<T> t_;
        CloneOnCopy<U> u_;
        CloneOnCopy<V> v_;
    };

    template <typename TFunc, typename T, typename U, typename V, typename W>
    class ArgumentBinder<TFunc, T, U, V, W>
    {
    public:
        ArgumentBinder(TFunc func, T t, U u, V v, W w) :
            func_(std::move(func)),
            t_(std::move(t)),
            u_(std::move(u)),
            v_(std::move(v)),
            w_(std::move(w))
        {
        }

        template <typename... Us>
        auto operator()(Us&&... us) const
        {
            return std::invoke(
                    *func_,
                    std::forward<Us>(us)...,
                    clone(*t_),
                    clone(*u_),
                    clone(*v_),
                    clone(*w_)
                    );
        }

    private:
        CloneOnCopy<TFunc> func_;
        CloneOnCopy<T> t_;
        CloneOnCopy<U> u_;
        CloneOnCopy<V> v_;
        CloneOnCopy<W> w_;
    };

    template <typename TFunc, typename T, typename U, typename V, typename W,
             typename X>
    class ArgumentBinder<TFunc, T, U, V, W, X>
    {
    public:
        ArgumentBinder(TFunc func, T t, U u, V v, W w, X x) :
            func_(std::move(func)),
            t_(std::move(t)),
            u_(std::move(u)),
            v_(std::move(v)),
            w_(std::move(w)),
            x_(std::move(x))
        {
        }

        template <typename... Us>
        auto operator()(Us&&... us) const
        {
            return std::invoke(
                    *func_,
                    std::forward<Us>(us)...,
                    clone(*t_),
                    clone(*u_),
                    clone(*v_),
                    clone(*w_),
                    clone(*x_)
                    );
        }

    private:
        CloneOnCopy<TFunc> func_;
        CloneOnCopy<T> t_;
        CloneOnCopy<U> u_;
        CloneOnCopy<V> v_;
        CloneOnCopy<W> w_;
        CloneOnCopy<X> x_;
    };

    template <typename TFunc, typename T, typename U, typename V, typename W,
             typename X, typename Y>
    class ArgumentBinder<TFunc, T, U, V, W, X, Y>
    {
    public:
        ArgumentBinder(TFunc func, T t, U u, V v, W w, X x, Y y) :
            func_(std::move(func)),
            t_(std::move(t)),
            u_(std::move(u)),
            v_(std::move(v)),
            w_(std::move(w)),
            x_(std::move(x)),
            y_(std::move(y))
        {
        }

        template <typename... Us>
        auto operator()(Us&&... us) const
        {
            return std::invoke(
                    *func_,
                    std::forward<Us>(us)...,
                    clone(*t_),
                    clone(*u_),
                    clone(*v_),
                    clone(*w_),
                    clone(*x_),
                    clone(*y_)
                    );
        }

    private:
        CloneOnCopy<TFunc> func_;
        CloneOnCopy<T> t_;
        CloneOnCopy<U> u_;
        CloneOnCopy<V> v_;
        CloneOnCopy<W> w_;
        CloneOnCopy<X> x_;
        CloneOnCopy<Y> y_;
    };


    template <typename TFunc, typename... Ts>
    auto bindArguments(TFunc&& func, Ts&&... ts)
    {
        return ArgumentBinder<std::decay_t<TFunc>, std::decay_t<Ts>...>(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );

    }
} // namespace btl

