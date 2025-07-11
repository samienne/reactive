#pragma once

#include "bq/bqvisibility.h"

#include <btl/uniqueid.h>
#include <btl/demangle.h>

#include <typeindex>
#include <memory>
#include <unordered_map>
#include <cassert>

namespace bq::signal
{
    BQ_EXPORT btl::UniqueId makeUniqueId();

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
            assert(data_.find(id) == data_.end());

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

