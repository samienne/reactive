#pragma once

#include "reactive/widget.h"

#include <btl/cloneoncopy.h>

#include <functional>
#include <tuple>

namespace reactive::widget
{
    template <typename... Ts>
    using WidgetTransformerFunc = std::function<
        std::pair<Widget, btl::CloneOnCopy<std::tuple<Ts...>>>(Widget)
        >;

    template <typename TFunc, typename... Ts>
    class WidgetTransformer;

    template <typename TFunc>
    auto makeWidgetTransformer(TFunc&& func);

    inline auto makeWidgetTransformer();

    namespace detail
    {
        template <typename T, typename...>
        struct GetWidgetTransformerFuncType
        {
            using type = T;
        };

        template <typename... Ts>
        struct GetWidgetTransformerFuncType<void, Ts...>
        {
            // void means type erasure
            using type = WidgetTransformerFunc<Ts...>;
        };


        template <typename T>
        inline constexpr bool isPair = false;

        template <typename T, typename U>
        inline constexpr bool isPair<std::pair<T, U>> = true;

        /*
        template <typename T>
        struct IsPair : std::false_type {};

        template <typename T, typename U>
        struct IsPair<std::pair<T, U>> : std::true_type {};
        */

        struct WidgetTransformerBuildTag {};

    } // namespace detail

    template <typename T>
    struct IsWidgetTransformer : std::false_type {};

    template <typename T, typename... Ts>
    struct IsWidgetTransformer<WidgetTransformer<T, Ts...>> : std::true_type {};

    template <typename TFunc, typename... Ts>
    class WidgetTransformer
    {
        using FuncType = typename detail::GetWidgetTransformerFuncType<
            TFunc, Ts...
            >::type;

    public:
        WidgetTransformer(detail::WidgetTransformerBuildTag&&, FuncType func) :
            func_(std::move(func))
        {
        }

        WidgetTransformer(WidgetTransformer const&) = default;
        WidgetTransformer(WidgetTransformer&&) = default;

        WidgetTransformer& operator=(WidgetTransformer const&) = default;
        WidgetTransformer& operator=(WidgetTransformer&&) = default;

        template <typename U>
        auto operator()(U widget)
        {
            //return std::move(func_)(std::move(widget));
            return func_(std::move(widget));
        }

        template <typename U>
        auto bind(U&& u) && // std::function<WidgetTransformer(Ts...)>
        {
            using UReturnType = std::invoke_result_t<U, Ts...>;
            static_assert(IsWidgetTransformer<UReturnType>::value);

            return makeWidgetTransformer(
                [f=std::move(func_), u=std::forward<U>(u)](auto w) mutable
                {
                    auto pair = std::move(f)(std::move(w));
                    auto transform = std::apply(std::move(u), std::move(*pair.second));

                    return std::move(transform)(std::move(pair.first));
                });
        }

        template <typename U, typename... Us>
        auto provide(WidgetTransformer<U, Us...> u) &&
        {
            static_assert(std::is_invocable_v<U, Widget>);
            using UReturnType = std::invoke_result_t<U, Widget>;
            static_assert(detail::isPair<UReturnType>);

            return makeWidgetTransformer(
                [f=std::move(func_), u=std::move(u)](auto w) mutable
                {
                    auto pair1 = std::move(f)(std::move(w));
                    auto pair2 = std::move(u)(std::move(pair1.first));

                    return std::make_pair(
                            std::move(pair2.first),
                            btl::cloneOnCopy(std::tuple_cat(
                                std::move(*pair1.second),
                                std::move(*pair2.second)
                                )
                            ));
                });
        }

        template <typename U, typename... Us, typename... Ws>
        auto provide(WidgetTransformer<U, Us...> u, Ws&&... ws) &&
        {
            return std::move(*this)
                .provide(std::move(u))
                .provide(std::forward<Ws>(ws)...)
                ;
        }

        template <typename... Us>
        auto values(Us&&... us) &&
        {
            return makeWidgetTransformer(
                [f=std::move(func_),
                us=btl::cloneOnCopy(std::make_tuple(std::forward<Us>(us)...))]
                (auto w) mutable
                {
                    auto pair = std::move(f)(std::move(w));

                    return std::make_pair(
                            std::move(pair.first),
                            btl::cloneOnCopy(std::tuple_cat(
                                std::move(*pair.second),
                                std::move(*us)
                                ))
                            );
                });
        }

        operator WidgetTransformer<void, Ts...>() &&
        {
            return WidgetTransformer<void>(
                    detail::WidgetTransformerBuildTag(),
                    std::move(func_)
                    );
        }

    private:
        FuncType func_;
    };

    namespace detail
    {
        template <typename TFunc, typename T>
        struct GetWidgetTransformerType {};

        template <typename TFunc, typename T, typename... Ts>
        struct GetWidgetTransformerType<
            TFunc,
            std::pair<T, btl::CloneOnCopy<std::tuple<Ts...>>>
            >
        {
            using type = WidgetTransformer<TFunc, Ts...>;
        };
    }

    template <typename TFunc>
    auto makeWidgetTransformer(TFunc&& func)
    {
        using ResultType = std::invoke_result_t<TFunc, Widget>;
        using WidgetTransformerType = typename detail::GetWidgetTransformerType<
            std::decay_t<TFunc>,
            ResultType
            >::type;

        return WidgetTransformerType(
                detail::WidgetTransformerBuildTag(),
                std::forward<TFunc>(func)
                );
    }

    inline auto makeWidgetTransformer()
    {
        return makeWidgetTransformer([](auto w)
            {
                return std::make_pair(
                        std::move(w),
                        btl::cloneOnCopy(std::tuple<>())
                        );
            });
    }

    template <typename... Ts>
    auto provideValues(Ts&&... ts)
    {
        return makeWidgetTransformer(
            [ts=btl::cloneOnCopy(std::make_tuple(std::forward<Ts>(ts)...))]
            (auto w) mutable
            {
                return std::make_pair(
                        std::move(w),
                        std::move(ts)
                        );
            });
    }

    template <typename TWidget, typename... Ts>
    auto makeWidgetTransformerResult(TWidget&& widget, Ts&&... ts)
    {
        static_assert(IsWidget<TWidget>::value);

        return std::make_pair(
                std::forward<TWidget>(widget),
                btl::cloneOnCopy(std::make_tuple(std::forward<Ts>(ts)...))
                );
    }
} // reactive::widget

