#pragma once

#include "merge.h"
#include "constant.h"
#include "frameinfo.h"
#include "signaltraits.h"
#include "updateresult.h"

#include <btl/any.h>
#include <btl/connection.h>

namespace reactive::signal2
{
    template <typename T, typename... Ts>
    class Signal;

    namespace detail
    {
        template <typename T>
        auto toSignal(T&& t)
        {
            return constant(std::forward<T>(t));
        }

        template <typename T, typename... Ts>
        Signal<T, Ts...> toSignal(Signal<T, Ts...> sig)
        {
            return std::move(sig);
        }
    } // anonymous namespace

    template <typename T>
    class Join
    {
    public:
        using OuterSignal = T;
        using OuterData = SignalDataTypeT<OuterSignal>;

    private:
        static auto makeInnerSignal(SignalTypeT<OuterSignal> outerResult)
        {
            return std::apply([](auto&&... ts)
                {
                    return merge(detail::toSignal(ts)...).unwrap();
                },
                std::move(outerResult).getTuple()
                );
        }
    public:

        using InnerSignal = std::decay_t<decltype(makeInnerSignal(std::declval<
                    SignalTypeT<OuterSignal>>()))>;

        using InnerData = SignalDataTypeT<InnerSignal>;

        struct DataType
        {
            DataType(T const& sig) :
                outerData(sig.initialize()),
                innerSignal(makeInnerSignal(sig.evaluate(outerData))),
                innerData(innerSignal.initialize())
            {
            }

            OuterData outerData;
            InnerSignal innerSignal;
            InnerData innerData;
            bool hasChanged = false;
        };

        Join(T sig) :
            sig_(std::move(sig))
        {
        }

        DataType initialize() const
        {
            return { sig_ };
        }

        auto evaluate(DataType const& data) const -> decltype(auto)
        {
            return data.innerSignal.evaluate(data.innerData);
        }

        bool hasChanged(DataType const& data) const
        {
            return data.hasChanged;
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            auto r = sig_.update(data.outerData, frame);
            if (r.didChange)
            {
                data.innerSignal = makeInnerSignal(sig_.evaluate(data.outerData));
                data.innerData = data.innerSignal.initialize();
            }

            r = r + data.innerSignal.update(data.innerData, frame);

            data.hasChanged = r.didChange;

            return r;
        }

        Connection observe(DataType& data, std::function<void()> callback)
        {
            auto c = sig_.observe(data.outerData, callback);
            c += data.innerSignal.observe(data.innerData, std::move(callback));

            return c;
        }

    private:
        T sig_;
    };

    template <typename T>
    struct IsSignal<Join<T>> : std::true_type {};

    template <typename T, typename... Ts>
    auto join(Signal<T, Ts...> sig)
    {
        if constexpr (btl::any(IsSignal<Ts>::value...))
        {
            return wrap(Join<T>(std::move(sig).unwrap()));
        }
        else
        {
            return sig;
        }
    }
} // namespace reactive::signal2
