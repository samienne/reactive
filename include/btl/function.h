#pragma once

#include <functional>

namespace btl
{
    template <typename T>
    class Function;

    template <typename TReturn, typename... Ts>
    class Function<TReturn(Ts...)>
    {
    public:
        template <typename F, typename = std::enable_if_t<
            std::is_convertible<F, std::function<TReturn(Ts...)>>::value &&
            !std::is_same<Function, std::decay_t<F>>::value
            >>
        Function(F&& func) :
            func_(std::forward<F>(func))
        {
        }

        Function(Function const&) = default;
        Function(Function&& rhs) noexcept :
            func_(std::move(rhs.func_))
        {
        }

        Function& operator=(Function const&) = default;
        Function& operator=(Function&& rhs) noexcept
        {
            func_ = std::move(rhs.func_);
            return *this;
        }

        template <typename F, typename = std::enable_if_t<
            std::is_convertible<F, std::function<TReturn(Ts...)>>::value
            >>
        Function& operator=(F&& f) noexcept
        {
            func_ = std::forward<F>(f);
            return *this;
        }

        template <typename... Us>
        auto operator()(Us&&... us) const
        -> std::result_of_t<std::function<TReturn(Ts...)>(Us...)>
        {
            return func_(std::forward<Us>(us)...);
        }

    private:
        std::function<TReturn(Ts...)> func_;
    };
} // reactive

