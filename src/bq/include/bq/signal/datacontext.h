#pragma once

#include "bq/bqvisibility.h"

#include <btl/uniqueid.h>
#include <btl/demangle.h>

#include <typeindex>
#include <memory>
#include <unordered_map>
#include <functional>
#include <cassert>

namespace bq::signal
{
    BQ_EXPORT btl::UniqueId makeUniqueId();

    /** Lifetime token for an observation. observe() ties every wakeup it
     * registers to one of these; the wakeups fire while it is alive and drop
     * when it does. */
    using ObserveGuard = std::shared_ptr<void>;

    /** Creates a fresh guard for a caller about to observe. */
    inline ObserveGuard makeObserveGuard()
    {
        return std::make_shared<char>();
    }

    /** A wake callback paired with the guard that scopes it.
     *
     * observe() stores these on the external leaves it reaches rather than
     * returning an unregistration handle: the callback fires only while its
     * guard is alive, so an observer disarms by dropping the guard. A leaf drops
     * a dead registration the next time it is observed or fires. */
    class ObserveCallback
    {
    public:
        ObserveCallback() = default;

        ObserveCallback(ObserveGuard const& guard, std::function<void()> callback) :
            guard_(guard),
            callback_(std::move(callback))
        {
        }

        /** True while the owning observer is still armed. */
        explicit operator bool() const
        {
            return !guard_.expired();
        }

        void operator()() const
        {
            if (auto alive = guard_.lock())
                callback_();
        }

    private:
        std::weak_ptr<void> guard_;
        std::function<void()> callback_;
    };

    class BQ_EXPORT DataContext
    {
    public:
        using DataId = btl::UniqueId;

        class Base
        {
        public:
            virtual ~Base() = default;
            virtual std::type_index getType() const = 0;
        };

        template <typename T>
        class Data : public Base
        {
        public:
            template <typename... Ts>
            Data(Ts&&... ts) :
                data_(std::forward<Ts>(ts)...)
            {
            }

            T& get()
            {
                return data_;
            }

            T const& get() const
            {
                return data_;
            }

            std::type_index getType() const override
            {
                return typeid(T);
            }

        private:
            T data_;
        };

        template <typename T, typename... Ts>
        static std::unique_ptr<Data<T>> makeData(Ts&&... args)
        {
            return std::make_unique<Data<T>>(std::forward<Ts>(args)...);
        }

        DataContext();
        btl::UniqueId getId() const;

        template <typename TData, typename... TArgs>
        std::shared_ptr<TData> initializeData(DataId id, TArgs&&... args)
        {
#ifndef NDEBUG
            // An entry whose data has been released stays in the map until it
            // is overwritten, so re-initializing an id is only an error while
            // the previous data is still alive.
            auto existing = data_.find(id);
            assert(existing == data_.end() || existing->second.expired());
#endif

            auto data = std::make_shared<Data<TData>>(std::forward<TArgs>(args)...);
            data_.insert_or_assign(id, std::weak_ptr<Base>(data));

            return std::shared_ptr<TData>(data, &data->get());
        }

        template <typename TData>
        std::shared_ptr<TData> findData(DataId id)
        {
            auto i = data_.find(id);
            if (i == data_.end())
                return nullptr;

            if (auto data = i->second.lock())
            {
                if (data->getType() != typeid(TData))
                {
                    throw std::runtime_error("findData: Type id mismatch ("
                            + std::to_string(id.getValue()) + "): "
                            + btl::demangle(data->getType().name()) + " != "
                            + btl::demangle(typeid(TData).name()));
                }

                auto typed = std::static_pointer_cast<Data<TData>>(data);
                return std::shared_ptr<TData>(typed, &typed->get());
            }

            return nullptr;
        }

        template <typename T>
        void storeFrameData(T&& data)
        {
            auto ptr = std::make_shared<Data<std::decay_t<T>>>(std::forward<T>(data));
            frameData_.push_back(ptr);
        }

        void swapFrameData()
        {
            prevFrameData_ = std::move(frameData_);
            frameData_.clear();
        }

    private:
        btl::UniqueId id_;
        std::unordered_map<DataId, std::weak_ptr<Base>> data_;
        std::vector<std::shared_ptr<Base>> frameData_;
        std::vector<std::shared_ptr<Base>> prevFrameData_;
    };
} // namespace bq::signal

