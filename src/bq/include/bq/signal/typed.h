#pragma once

#include "signalresult.h"
#include "updateresult.h"
#include "frameinfo.h"
#include "signaltraits.h"
#include "datacontext.h"

#include <btl/connection.h>

#include <memory>

namespace bq::signal
{
    template <typename... Ts>
    class SignalBase
    {
    public:
        struct DataBase
        {
            virtual ~DataBase() = default;
        };

        struct DataType
        {
            std::unique_ptr<DataBase> data;
        };

        virtual ~SignalBase() = default;

        virtual std::unique_ptr<DataBase> initialize(DataContext& context,
                FrameInfo const& frame) const = 0;
        virtual SignalResult<Ts...> evaluate(DataContext& context,
                DataBase const& data) const = 0;
        virtual UpdateResult update(DataContext& context, DataBase& data,
                FrameInfo const& frame) = 0;
        virtual btl::connection observe(DataContext& context, DataBase& data,
                std::function<void()> callback) = 0;
    };

    template <typename TStorage, typename... Ts>
    class SignalTyped : public SignalBase<Ts...>
    {
    public:
        using BaseDataType = typename SignalBase<Ts...>::DataBase;
        using StorageDataType = typename TStorage::DataType;

        struct DataType : BaseDataType
        {
            DataType(typename TStorage::DataType data) :
                data(std::move(data))
            {
            }

            StorageDataType data;
        };

        SignalTyped(TStorage sig) :
            sig_(std::move(sig))
        {
        }

        std::unique_ptr<BaseDataType> initialize(DataContext& context,
                FrameInfo const& frame) const override
        {
            return { std::make_unique<DataType>(sig_.initialize(context, frame)) };
        }

        SignalResult<Ts...> evaluate(DataContext& context,
                BaseDataType const& baseData) const override
        {
            return sig_.evaluate(context, getStorageData(baseData));
        }

        UpdateResult update(DataContext& context, BaseDataType& baseData,
                FrameInfo const& frame) override
        {
            return sig_.update(context, getStorageData(baseData), frame);
        }

        btl::connection observe(DataContext& context, BaseDataType& data,
                std::function<void()> callback) override
        {
            return sig_.observe(context, getStorageData(data), std::move(callback));
        }

    private:
        static StorageDataType& getStorageData(BaseDataType& data)
        {
            return static_cast<DataType&>(data).data;
        }

        static StorageDataType const& getStorageData(BaseDataType const& data)
        {
            return static_cast<DataType const&>(data).data;
        }

    private:
        TStorage sig_;
    };

    template <typename... Ts>
    class SignalTypeless
    {
    public:
        using DataType = typename SignalBase<Ts...>::DataType;

        SignalTypeless(std::shared_ptr<SignalBase<Ts...>> sig) :
            sig_(std::move(sig))
        {
        }

        SignalTypeless(SignalTypeless const&) = default;
        SignalTypeless(SignalTypeless&&) noexcept = default;

        SignalTypeless& operator=(SignalTypeless const&) = default;
        SignalTypeless& operator=(SignalTypeless&&) noexcept = default;

        DataType initialize(DataContext& context, FrameInfo const& frame) const
        {
            return { sig_->initialize(context, frame) };
        }

        auto evaluate(DataContext& context, DataType const& data) const
        {
            return sig_->evaluate(context, *data.data);
        }

        UpdateResult update(DataContext& context, DataType& data,
                FrameInfo const& frame)
        {
            return sig_->update(context, *data.data, frame);
        }

        template <typename TCallback>
        btl::connection observe(DataContext& context, DataType& data,
                TCallback&& callback)
        {
            return sig_->observe(context, *data.data, std::forward<TCallback>(callback));
        }

        template <typename... Us, typename = std::enable_if_t<
            btl::all(std::is_convertible_v<Ts, Us>...)
            >>
        SignalTypeless<Us...> cast() const
        {
            return { std::make_shared<SignalTyped<SignalTypeless<Ts...>, Us...>>>(
                    *this) };
        }

    private:
        std::shared_ptr<SignalBase<Ts...>> sig_;
    };

    template <typename... Ts>
    struct IsSignal<SignalTypeless<Ts...>> : std::true_type {};

    template <typename TStorage, typename... Ts>
    class Signal;

    template <typename... Ts, typename TStorage, typename... Us, typename =
        std::enable_if_t<btl::all(std::is_convertible_v<Us, Ts>...)>
        >
    auto makeTypedSignal(Signal<TStorage, Us...> sig)
    {
        using StorageType = std::decay_t<decltype(sig.unwrap())>;
        return std::make_shared<SignalTyped<StorageType, Ts...>>(sig.unwrap());
    }

    template <typename... Ts, typename TStorage, typename... Us, typename =
        std::enable_if_t<btl::all(std::is_convertible_v<Us, Ts>...)>
        >
    auto makeTypelessSignal(Signal<TStorage, Us...> sig)
    {
        using StorageType = std::decay_t<decltype(sig.unwrap())>;
        if constexpr (std::is_same_v<StorageType, SignalTypeless<Ts...>>)
        {
            return std::move(sig).unwrap();
        }
        else
        {
            return SignalTypeless<Ts...>(
                    makeTypedSignal<Ts...>(std::move(sig))
                    );
        }
    }
} // namespace bq::signal

