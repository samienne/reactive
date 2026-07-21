#pragma once

#include "../datacontext.h"
#include "../frameinfo.h"
#include "../signal.h"
#include "../signalresult.h"
#include "../signaltraits.h"
#include "../updateresult.h"
#include "../wrap.h"

#include <btl/connection.h>
#include <btl/uniqueid.h>

#include <cassert>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace bq::signal::detail
{
    /** @brief Signal of the element stored under one key of a keyed source.
     *
     * Reads a fixed key out of a signal of `std::map<TKey, TValue>`. The key
     * need not be present at every update: while it is absent the signal holds
     * the last value it saw, so it always has a value to report and reports no
     * change until the key returns.
     *
     * The latched value is per-context state, looked up by a UniqueId carried
     * in the description, so every instantiation of one description into one
     * context latches together while two contexts stay independent.
     */
    template <typename TKey, typename TValue, typename TSignal>
    class Pick
    {
    public:
        struct ContextDataType
        {
            ContextDataType(TValue value) :
                value(std::move(value))
            {
            }

            TValue value;
        };

        struct DataType
        {
            typename TSignal::DataType signalData;
            std::shared_ptr<ContextDataType> contextData;
        };

        Pick(TSignal sig, TKey key, btl::UniqueId id) :
            sig_(std::move(sig)),
            key_(std::move(key)),
            id_(id)
        {
        }

        /** @brief Initializes the signal into @p context.
         *
         * @throws std::runtime_error if the key is absent from the source and
         *         the context holds no earlier value for it.
         */
        DataType initialize(DataContext& context, FrameInfo const& frame) const
        {
            auto signalData = sig_.initialize(context, frame);

            auto contextData = context.findData<ContextDataType>(id_);
            if (!contextData)
            {
                auto const& keyed = sig_.evaluate(context, signalData)
                    .template get<0>();

                auto i = keyed.find(key_);
                if (i == keyed.end())
                    throw std::runtime_error(
                            "pick: key is absent and nothing has been latched");

                contextData = context.initializeData<ContextDataType>(id_,
                        i->second);
            }

            return { std::move(signalData), std::move(contextData) };
        }

        SignalResult<TValue const&> evaluate(DataContext&,
                DataType const& data) const
        {
            return SignalResult<TValue const&>(data.contextData->value);
        }

        UpdateResult update(DataContext& context, DataType& data,
                FrameInfo const& frame)
        {
            UpdateResult result = sig_.update(context, data.signalData, frame);

            if (!result.didChange)
                return result;

            auto const& keyed = sig_.evaluate(context, data.signalData)
                .template get<0>();

            auto i = keyed.find(key_);
            if (i == keyed.end())
            {
                result.didChange = false;
                return result;
            }

            data.contextData->value = i->second;

            return result;
        }

        template <typename TCallback>
        btl::connection observe(DataContext& context, DataType& data,
                TCallback&& callback)
        {
            return sig_.observe(context, data.signalData,
                    std::forward<TCallback>(callback));
        }

    private:
        TSignal sig_;
        TKey key_;
        btl::UniqueId id_;
    };

    /** @brief Builds a signal of one element of a keyed source.
     *
     * @param source A keyed source, normally the result of shareKeyed().
     * @param key    The key to follow. It is carried by value and is the only
     *               thing that distinguishes one pick from another.
     */
    template <typename TStorage, typename TKey, typename TValue>
    auto pick(Signal<TStorage, std::map<TKey, TValue>> source, TKey key)
    {
        using Storage = SignalStorageType<TStorage, std::map<TKey, TValue>>;

        return wrap(Pick<TKey, TValue, Storage>(
                    std::move(source).unwrap(),
                    std::move(key),
                    makeUniqueId()
                    ));
    }

    /** @brief Shares a signal of a vector as a signal of a key to element map.
     *
     * Keying rather than sharing the vector itself is what keeps a pick a
     * lookup instead of a scan, so that following every element of a source
     * costs O(n log n) per update rather than O(n^2).
     *
     * @param keyFn Maps an element to its key. Keys must be unique within one
     *              source; a duplicate is a debug assert and the first
     *              occurrence wins.
     */
    template <typename TStorage, typename T, typename TFunc>
    auto shareKeyed(Signal<TStorage, std::vector<T>> source, TFunc&& keyFn)
    {
        using TKey = std::decay_t<std::invoke_result_t<TFunc&, T const&>>;

        return std::move(source)
            .map([keyFn=std::forward<TFunc>(keyFn)]
                    (std::vector<T> const& items) mutable
                {
                    std::map<TKey, T> keyed;

                    for (auto const& item : items)
                    {
                        auto inserted = keyed.insert({ keyFn(item), item });
                        assert(inserted.second);
                        (void) inserted;
                    }

                    return keyed;
                })
            .share();
    }
} // namespace bq::signal::detail
