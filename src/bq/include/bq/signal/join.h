#pragma once

#include "merge.h"
#include "constant.h"
#include "frameinfo.h"
#include "signaltraits.h"
#include "updateresult.h"

#include <btl/any.h>
#include <btl/connection.h>

namespace reactive::signal
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
            return sig;
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
            DataType(DataContext& context, T const& sig, FrameInfo const& frame) :
                outerData(sig.initialize(context, frame)),
                innerSignal(makeInnerSignal(sig.evaluate(context, outerData))),
                innerData(innerSignal.initialize(context, frame))
            {
            }

            OuterData outerData;
            InnerSignal innerSignal;
            InnerData innerData;
        };

        Join(T sig) :
            sig_(std::move(sig))
        {
        }

        DataType initialize(DataContext& context, FrameInfo const& frame) const
        {
            return { context, sig_, frame };
        }

        auto evaluate(DataContext& context, DataType const& data) const -> decltype(auto)
        {
            return data.innerSignal.evaluate(context, data.innerData);
        }

        UpdateResult update(DataContext& context, DataType& data, FrameInfo const& frame)
        {
            auto r = sig_.update(context, data.outerData, frame);
            if (r.didChange)
            {
                data.innerSignal = makeInnerSignal(sig_.evaluate(context, data.outerData));
                data.innerData = data.innerSignal.initialize(context, frame);
            }

            r = r + data.innerSignal.update(context, data.innerData, frame);

            return r;
        }

        Connection observe(DataContext& context, DataType& data,
                std::function<void()> callback)
        {
            auto c = sig_.observe(context, data.outerData, callback);
            c += data.innerSignal.observe(context, data.innerData, std::move(callback));

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
} // namespace reactive::signal
