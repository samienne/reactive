#pragma once

#include "wrap.h"
#include "frameinfo.h"
#include "updateresult.h"
#include "signaltraits.h"
#include "signalresult.h"

#include <btl/copywrapper.h>

namespace reactive::signal
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

        DataType initialize() const
        {
            return {};
        }

        SignalResult<T const&> evaluate(DataType const&) const
        {
            return SignalResult<T const&>(*constant_);
        }

        bool hasChanged(DataType const&) const
        {
            return false;
        }

        UpdateResult update(DataType&, FrameInfo const&)
        {
            return { std::nullopt, false };
        }

        template <typename TCallback>
        Connection observe(DataType&, TCallback&&)
        {
            // nothing to observe
            return Connection();
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
} // reactive::signal
