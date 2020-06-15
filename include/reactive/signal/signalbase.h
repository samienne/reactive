#pragma once

#include "signaltraits.h"
#include "signalresult.h"

#include "reactive/connection.h"

#include <memory>

namespace reactive::signal
{
    class FrameInfo;

    namespace detail
    {
        template <typename... Ts>
        struct SignalBaseResult
        {
            using type = SignalResult<Ts...>;
        };

        template <typename T>
        struct SignalBaseResult<T>
        {
            using type = T;
        };
    } // namespace detail

    template <typename... Ts>
    class SignalBase
    {
    public:
        virtual ~SignalBase() = default;
        virtual typename detail::SignalBaseResult<Ts...>::type evaluate() const = 0;
        virtual bool hasChanged() const = 0;
        virtual Connection observe(std::function<void()> const& callback) = 0;
        virtual UpdateResult updateBegin(FrameInfo const&) = 0;
        virtual UpdateResult updateEnd(FrameInfo const&) = 0;
        virtual Annotation annotate() const = 0;
        virtual bool isCached() const = 0;
    };
} // reactive::signal

