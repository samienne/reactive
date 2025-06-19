#pragma once

#include "all.h"

#include <variant>

namespace btl
{
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
            return std::holds_alternative<TValue>(value_);
        }

        TValue const& value() const&
        {
            return std::get<TValue>(value_);
        }

        TValue value() &&
        {
            return std::move(std::get<TValue>(value_));
        }

        TValue& value() &
        {
            return std::get<TValue>(value_);
        }

        TErr const& err() const&
        {
            return std::get<TErr>(value_);
        }

        TErr err() &&
        {
            return std::move(std::get<TErr>(value_));
        }

        TErr& err() &
        {
            return std::get<TErr>(value_);
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
        std::variant<TValue, TErr> value_;
    };
} // btl

