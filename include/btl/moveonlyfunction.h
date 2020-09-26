#pragma once

#include <functional>
#include <cassert>

namespace btl
{
    template <typename T>
    struct AssertOnCopy
    {
        AssertOnCopy(T t) :
            value(std::move(t))
        {
        }

        AssertOnCopy(AssertOnCopy const& rhs) noexcept :
            value(std::move(const_cast<AssertOnCopy&>(rhs).value))
        {
            assert(false && "Copy disabled at runtime");
        }

        AssertOnCopy(AssertOnCopy&& rhs) noexcept :
            value(std::move(rhs.value))
        {
        }

        AssertOnCopy& operator=(AssertOnCopy const& rhs) noexcept
        {
            assert(false && "Copy disabled at runtime");
            value = std::move(const_cast<AssertOnCopy>(rhs).value);
            return *this;
        }

        AssertOnCopy& operator=(AssertOnCopy&& rhs) noexcept
        {
            value = std::move(rhs.value);
            return *this;
        }

        std::decay_t<T> value;
    };

    template <typename T>
    auto assertOnCopy(T&& t)
    {
        return AssertOnCopy<std::decay_t<T>>(std::forward<T>(t));
    }

    template <typename TSignature>
    class MoveOnlyFunction
    {
    public:
        template <typename TCallable, typename = std::enable_if_t
            <
                std::is_convertible<
                    TCallable, std::function<TSignature>>::value
                >
            >
        MoveOnlyFunction(TCallable&& callable) :
            func_([callable=assertOnCopy(std::forward<TCallable>(callable))]
                        (auto&&... values) mutable noexcept
            {
                return std::invoke(std::move(callable.value),
                    std::forward<decltype(values)>(values)...);
            })
        {
        }

        MoveOnlyFunction(MoveOnlyFunction const&) = delete;

        // std::function is not nothrow move constructible
        MoveOnlyFunction(MoveOnlyFunction&& rhs) noexcept :
            func_(std::move(rhs.func_))
        {
        }

        // std::function is not nothrow move assignable
        MoveOnlyFunction& operator=(MoveOnlyFunction&& rhs) noexcept
        {
            func_ = std::move(rhs.func_);

            return *this;
        }

        template <typename... Ts>
        auto operator()(Ts&&... ts) const
        -> std::invoke_result_t<std::function<TSignature>, Ts...>
        {
            return func_(std::forward<Ts>(ts)...);
        }

        template <typename... Ts>
        auto operator()(Ts&&... ts)
        -> std::invoke_result_t<std::function<TSignature>, Ts...>
        {
            return func_(std::forward<Ts>(ts)...);
        }

    private:
        std::function<TSignature> func_;
    };
} // btl

