#pragma once

#include "signal.h"
#include "datacontext.h"

#include "reactive/connection.h"

namespace reactive::signal
{
    template <typename T>
    class Combine
    {
    public:
        struct DataType
        {
            std::vector<SignalDataTypeT<AnySignal<T>>> datas;
        };

        Combine(std::vector<AnySignal<T>> sigs) :
            sigs_(std::move(sigs))
        {
        }

        DataType initialize(DataContext& context) const
        {
            std::vector<SignalDataTypeT<AnySignal<T>>> datas;
            for (auto const& sig : sigs_)
                datas.push_back(sig.unwrap().initialize(context));

            return { std::move(datas) };
        }

        SignalResult<std::vector<T>> evaluate(DataContext& context,
                DataType const& data) const
        {
            std::vector<T> result;

            for (size_t i = 0; i < sigs_.size(); ++i)
                result.push_back(sigs_[i].unwrap().evaluate(context,
                            data.datas[i]).template get<0>());

            return SignalResult<std::vector<T>>{ std::move(result) };
        }

        UpdateResult update(DataContext& context, DataType& data, FrameInfo const& frame)
        {
            UpdateResult r;
            for (size_t i = 0; i < sigs_.size(); ++i)
                r = r + sigs_[i].unwrap().update(context, data.datas[i], frame);

            return r;
        }

        Connection observe(DataContext& context, DataType& data,
                std::function<void()> callback)
        {
            Connection c;

            for (size_t i = 0; i < sigs_.size(); ++i)
                c += sigs_[i].unwrap().observe(context, data.datas[i], callback);

            return c;
        }

    private:
        std::vector<AnySignal<T>> sigs_;
    };

    template <typename T>
    struct IsSignal<Combine<T>> : std::true_type {};

    template <typename T>
    auto combine(std::vector<AnySignal<T>> sigs)
    {
        return wrap(Combine<T>(std::move(sigs)));
    }
} // namespace reactive2::signal
