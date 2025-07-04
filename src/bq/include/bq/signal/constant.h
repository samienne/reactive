#pragma once

#include "wrap.h"
#include "frameinfo.h"
#include "updateresult.h"
#include "signaltraits.h"
#include "signalresult.h"
#include "datacontext.h"

#include <btl/copywrapper.h>

namespace bq::signal
{
    template <typename T>
    class Constant
    {
    public:
        struct DataType
        {
        };

        Constant(Constant<T> const&) = default;
        Constant(Constant<T>&&) = default;

        Constant<T>& operator=(Constant<T> const&) = default;
        Constant<T>& operator=(Constant<T>&&) = default;

        Constant(T&& initial) :
            constant_(std::move(initial))
        {
        }

        Constant(T const& initial) :
            constant_(initial)
        {
        }

        DataType initialize(DataContext&, FrameInfo const&) const
        {
            return {};
        }

        SignalResult<T const&> evaluate(DataContext&, DataType const&) const
        {
            return SignalResult<T const&>(*constant_);
        }

        UpdateResult update(DataContext&, DataType&, FrameInfo const&)
        {
            return { std::nullopt, false };
        }

        template <typename TCallback>
        btl::connection observe(DataContext&, DataType&, TCallback&&)
        {
            // nothing to observe
            return btl::connection();
        }

    private:
        btl::CopyWrapper<T> constant_;
    };

    template <typename T>
    auto constant(T&& value)
    {
        return wrap(Constant<std::decay_t<T>>(
                    std::forward<T>(value)
                    ));
    }

    template <typename T>
    auto constant(std::initializer_list<T> v)
    {
        return constant(std::vector<std::decay_t<T>>(v));
    }

    template <typename... Ts>
    struct IsSignal<Constant<Ts...>> : std::true_type {};
} // bq::signal
