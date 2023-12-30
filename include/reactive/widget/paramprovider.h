#pragma once

#include "buildparams.h"
#include <type_traits>

namespace reactive::widget
{
    struct ParamProviderBuildTag
    {
    };

    template <typename TFunc>
    auto makeParamProviderUnchecked(TFunc&& func);

    template <typename TFunc>
    class ParamProvider
    {
    public:
        ParamProvider(ParamProviderBuildTag const&, TFunc func) :
            func_(std::move(func))
        {
        }


        auto operator()(BuildParams const& params) const
        {
            return std::invoke(
                    func_,
                    params
                    );
        }

        template <typename UFunc>
        auto map(UFunc&& func) const
        {
            return makeParamProviderUnchecked(
                [provider=func_, func=std::forward<UFunc>(func)]
                (BuildParams const& params)
                {
                    return std::invoke(
                            func,
                            provider(params)
                            );
                });
        }

    private:
        TFunc func_;
    };

    template <typename T>
    struct ParamProviderType
    {
        using type = T;
    };

    template <typename T>
    struct ParamProviderType<ParamProvider<T>>
    {
        using type = decltype(std::declval<ParamProvider<T>>()(BuildParams()));
    };

    template <typename T>
    struct ParamProviderType<ParamProvider<T> const&>
    {
        using type = decltype(std::declval<ParamProvider<T> const&>()(BuildParams()));
    };

    template <typename T>
    struct ParamProviderType<ParamProvider<T>&&>
    {
        using type = decltype(std::declval<ParamProvider<T>&&>()(BuildParams()));
    };

    template <typename T>
    using ParamProviderTypeT = typename ParamProviderType<T>::type;

    template <typename TFunc>
    auto makeParamProviderUnchecked(TFunc&& func)
    {
        return ParamProvider<std::decay_t<TFunc>>(
                ParamProviderBuildTag{},
                std::forward<TFunc>(func)
                );
    }

    template <typename TFunc, typename = std::enable_if_t<
        std::is_invocable_v<TFunc, BuildParams const&>
        >>
    auto makeParamProvider(TFunc&& func)
    {
        return makeParamProviderUnchecked(std::forward<TFunc>(func));
    }

    template <typename T>
    auto invokeParamProvider(ParamProvider<T> const& provider, BuildParams const& params)
    {
        return std::invoke(provider, params);
    }

    template <typename T>
    auto invokeParamProvider(ParamProvider<T>& provider, BuildParams const& params)
    {
        return std::invoke(provider, params);
    }

    template <typename T>
    auto invokeParamProvider(ParamProvider<T>&& provider, BuildParams const& params)
    {
        return std::invoke(std::move(provider), params);
    }

    template <typename T>
    auto invokeParamProvider(T&& t, BuildParams const&)
    {
        return std::forward<T>(t);
    }
}
