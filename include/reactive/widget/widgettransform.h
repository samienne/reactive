#pragma once

#include "reactive/widget.h"

#include <btl/cloneoncopy.h>

#include <functional>
#include <tuple>

namespace reactive::widget
{
    template <typename... Ts>
    using WidgetTransformFunc = std::function<
        std::pair<Widget, btl::CloneOnCopy<std::tuple<Ts...>>>(Widget)
        >;

    template <typename TFunc, typename... Ts>
    class WidgetTransform;

    template <typename TFunc>
    auto makeWidgetTransform(TFunc&& func);

    inline auto makeWidgetTransform();

    namespace detail
    {
        template <typename T, typename...>
        struct GetWidgetTransformFuncType
        {
            using type = T;
        };

        template <typename... Ts>
        struct GetWidgetTransformFuncType<void, Ts...>
        {
            // void means type erasure
            using type = WidgetTransformFunc<Ts...>;
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

        struct WidgetTransformBuildTag {};

    } // namespace detail

    template <typename T>
    struct IsWidgetTransform : std::false_type {};

    template <typename T, typename... Ts>
    struct IsWidgetTransform<WidgetTransform<T, Ts...>> : std::true_type {};

    template <typename TFunc, typename... Ts>
    class WidgetTransform
    {
        using FuncType = typename detail::GetWidgetTransformFuncType<TFunc, Ts...>::type;

    public:
        WidgetTransform(detail::WidgetTransformBuildTag&&, FuncType func) :
            func_(std::move(func))
        {
        }

        WidgetTransform(WidgetTransform const&) = default;
        WidgetTransform(WidgetTransform&&) = default;

        WidgetTransform& operator=(WidgetTransform const&) = default;
        WidgetTransform& operator=(WidgetTransform&&) = default;

        template <typename U>
        auto operator()(U widget)
        {
            //return std::move(func_)(std::move(widget));
            return func_(std::move(widget));
        }

        template <typename U>
        auto bind(U&& u) && // std::function<WidgetTransform(Ts...)>
        {
            using UReturnType = std::invoke_result_t<U, Ts...>;
            static_assert(IsWidgetTransform<UReturnType>::value);

            return makeWidgetTransform(
                [f=std::move(func_), u=std::forward<U>(u)](auto w) mutable
                {
                    auto pair = std::move(f)(std::move(w));
                    auto transform = std::apply(std::move(u), std::move(*pair.second));

                    return std::move(transform)(std::move(pair.first));
                });
        }

        template <typename U, typename... Us>
        auto provide(WidgetTransform<U, Us...> u) &&
        {
            static_assert(std::is_invocable_v<U, Widget>);
            using UReturnType = std::invoke_result_t<U, Widget>;
            static_assert(detail::isPair<UReturnType>);

            return makeWidgetTransform(
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
        auto provide(WidgetTransform<U, Us...> u, Ws&&... ws) &&
        {
            return std::move(*this)
                .provide(std::move(u))
                .provide(std::forward<Ws>(ws)...)
                ;
        }

        template <typename... Us>
        auto values(Us&&... us) &&
        {
            return makeWidgetTransform(
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

        operator WidgetTransform<void, Ts...>() &&
        {
            return WidgetTransform<void>(
                    detail::WidgetTransformBuildTag(),
                    std::move(func_)
                    );
        }

    private:
        FuncType func_;
    };

    namespace detail
    {
        template <typename TFunc, typename T>
        struct GetWidgetTransformType {};

        template <typename TFunc, typename T, typename... Ts>
        struct GetWidgetTransformType<
            TFunc,
            std::pair<T, btl::CloneOnCopy<std::tuple<Ts...>>>
            >
        {
            using type = WidgetTransform<TFunc, Ts...>;
        };
    }

    template <typename TFunc>
    auto makeWidgetTransform(TFunc&& func)
    {
        using ResultType = std::invoke_result_t<TFunc, Widget>;
        using WidgetTransformType = typename detail::GetWidgetTransformType<
            std::decay_t<TFunc>,
            ResultType
            >::type;

        return WidgetTransformType(
                detail::WidgetTransformBuildTag(),
                std::forward<TFunc>(func)
                );
    }

    inline auto makeWidgetTransform()
    {
        return makeWidgetTransform([](auto w)
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
        return makeWidgetTransform(
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
    auto makeWidgetTransformResult(TWidget&& widget, Ts&&... ts)
    {
        static_assert(IsWidget<TWidget>::value);

        return std::make_pair(
                std::forward<TWidget>(widget),
                btl::cloneOnCopy(std::make_tuple(std::forward<Ts>(ts)...))
                );
    }
} // reactive::widget

