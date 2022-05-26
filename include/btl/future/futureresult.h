#pragma once

#include <tuple>

#include <cstddef>

namespace btl::future
{

template <typename... Ts>
class FutureResult
{
public:
    FutureResult(std::tuple<Ts...> values) :
        values_(std::move(values))
    {
    }

    template <size_t I>
    auto& get() &
    {
        return std::get<values_>();
    }

    template <size_t I>
    auto const& get() const&
    {
        return std::get<values_>();
    }

    template <size_t I>
    auto&& get() &&
    {
        return std::get<std::move(values_)>();
    }

    auto& getAsTuple() &
    {
        return values_;
    }

    auto const& getAsTuple() const&
    {
        return values_;
    }

    auto&& getAsTuple() &&
    {
        return std::move(values_);
    }

private:
    std::tuple<Ts...> values_;
};

template <typename T>
struct IsFutureResult : std::false_type {};

template <typename... Ts>
struct IsFutureResult<FutureResult<Ts...>> : std::true_type {};

template <typename... Ts>
auto makeFutureResult(Ts&&... ts) -> FutureResult<std::decay_t<Ts>...>
{
    return FutureResult<std::decay_t<Ts>...>(std::make_tuple(std::forward<Ts>(ts)...));
}

} // namespace btl::future

