#pragma once

#include "reactive/reactivevisibility.h"

#include <btl/uniqueid.h>
#include <btl/demangle.h>

#include <typeindex>
#include <memory>
#include <unordered_map>
#include <cassert>

namespace reactive::signal
{
    REACTIVE_EXPORT btl::UniqueId makeUniqueId();

    class REACTIVE_EXPORT DataContext
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

    private:
        btl::UniqueId id_;
        std::unordered_map<DataId, std::weak_ptr<Base>> data_;
    };
} // namespace reactive::signal

