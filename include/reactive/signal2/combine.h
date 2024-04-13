#pragma once

#include "signal.h"

#include "reactive/connection.h"

namespace reactive::signal2
{
    template <typename T>
    class Combine
    {
    public:
        struct DataType
        {
            std::vector<SignalDataTypeT<AnySignal<T>>> datas;
            bool hasChanged = false;
        };

        Combine(std::vector<AnySignal<T>> sigs) :
            sigs_(std::move(sigs))
        {
        }

        DataType initialize() const
        {
            std::vector<SignalDataTypeT<AnySignal<T>>> datas;
            for (auto const& sig : sigs_)
                datas.push_back(sig.unwrap().initialize());

            return { std::move(datas), false };
        }

        SignalResult<std::vector<T>> evaluate(DataType const& data) const
        {
            std::vector<T> result;

            for (size_t i = 0; i < sigs_.size(); ++i)
                result.push_back(sigs_[i].unwrap().evaluate(data.datas[i])
                        .template get<0>());

            return SignalResult<std::vector<T>>{ std::move(result) };
        }

        bool hasChanged(DataType const& data) const
        {
            return data.hasChanged;
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            UpdateResult r;
            for (size_t i = 0; i < sigs_.size(); ++i)
                r = r + sigs_[i].unwrap().update(data.datas[i], frame);

            data.hasChanged = r.didChange;

            return r;
        }

        Connection observe(DataType& data, std::function<void()> callback)
        {
            Connection c;

            for (size_t i = 0; i < sigs_.size(); ++i)
                c += sigs_[i].unwrap().observe(data.datas[i], callback);

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
} // namespace reactive2::signal2
