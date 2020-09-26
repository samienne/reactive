#pragma once

#include "all.h"
#include "variant.h"

namespace btl
{
    /*
    template <typename TValue, typename TErr>
    class Result;

    template <typename T>
    struct IsResult : std::false_type {};

    template <typename T, typename U>
    struct IsResult<Result<T, U>> : std::true_type {};

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        All<IsResult, Ts...>::value
        >
    >
    auto fmap(TFunc&& func, Ts&&... ts)
    -> Result<
        std::invoke_result_t<TFunc&&, typename Ts::ValueType...>,
        std::common_type_t<typename Ts::ErrorType...>
    >
    {
        if (all(!std::forward<Ts>(ts).valid()...))
            return 

        return std::forward<TFunc>(func)(std::forward<Ts>(ts).value()...);
    }
    */

    template <typename TValue, typename TErr>
    class Result
    {
    public:
        using ValueType = TValue;
        using ErrorType = TErr;

        Result(TValue&& value) :
            value_(std::move(value))
        {
        }

        Result(TValue const& value) :
            value_(std::move(value))
        {
        }

        Result(TErr&& err) :
            value_(std::move(err))
        {
        }

        Result(TErr const& err) :
            value_(err)
        {
        }

        bool valid() const noexcept
        {
            return value_.template is<TValue>();
        }

        TValue const& value() const&
        {
            return value_.template get<TValue>();
        }

        TValue value() &&
        {
            return std::move(value_.template get<TValue>());
        }

        TValue& value() &
        {
            return value_.template get<TValue>();
        }

        TErr const& err() const&
        {
            return value_.template get<TErr>();
        }

        TErr err() &&
        {
            return std::move(value_.template get<TErr>());
        }

        TErr& err() &
        {
            return value_.template get<TErr>();
        }

        template <typename TFunc>
        Result<std::decay_t<std::invoke_result_t<TFunc, TValue&&>>, TErr> fmap(
                TFunc&& func) &&
        {
            if (!valid())
                return std::move(*this).err();

            return std::forward<TFunc>(func)(std::move(*this).value());
        }

        template <typename TFunc>
        Result<std::decay_t<std::invoke_result_t<TFunc, TValue const&>>, TErr> fmap(
                TFunc&& func) const&
        {
            if (!valid())
                return err();

            return std::forward<TFunc>(func)(value());
        }

        template <typename TFunc>
        Result<std::decay_t<std::invoke_result_t<TFunc, TValue>>, TErr> fmap(
                TFunc&& func) &
        {
            if (!valid())
                return err();

            return std::forward<TFunc>(func)(value());
        }

        template <typename TFunc>
        Result<TValue, std::decay_t<std::invoke_result_t<TFunc, TErr&&>>> errMap(
                TFunc&& func) &&
        {
            if (valid())
                return std::move(*this).value();

            return std::forward<TFunc>(func)(std::move(*this).err());
        }

        template <typename TFunc>
        Result<TValue, std::decay_t<std::invoke_result_t<TFunc, TErr const&>>> errMap(
                TFunc&& func) const&
        {
            if (valid())
                return value();

            return std::forward<TFunc>(func)(err());
        }

        template <typename TFunc>
        Result<TValue, std::decay_t<std::invoke_result_t<TFunc, TErr&>>> errMap(
                TFunc&& func) &
        {
            if (valid())
                return value();

            return std::forward<TFunc>(func)(err());
        }

    private:
        btl::variant<TValue, TErr> value_;
    };
} // btl

