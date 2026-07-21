#pragma once

#include "bq/signal/datacontext.h"
#include "bq/signal/frameinfo.h"
#include "bq/signal/signal.h"
#include "bq/signal/signalresult.h"
#include "bq/signal/signaltraits.h"
#include "bq/signal/updateresult.h"
#include "bq/signal/wrap.h"

#include <btl/connection.h>
#include <btl/uniqueid.h>

#include <cassert>
#include <map>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace bq::signal::detail
{
    /** @brief A source's elements arranged for lookup by key.
     *
     * Held behind a shared pointer because a shared signal copies its value
     * into every consumer's data on every changed frame. Passing the map by
     * value would make that copy O(n) per consumer, so following all n
     * elements of one source would cost O(n^2) per update in copies alone.
     */
    template <typename TKey, typename TValue>
    using KeyedElements = std::shared_ptr<std::map<TKey, TValue> const>;

    /** @brief Blocks template argument deduction for a parameter. */
    template <typename T>
    struct Identity
    {
        using type = T;
    };

    template <typename T>
    using IdentityT = typename Identity<T>::type;

    /** @brief Signal of the element stored under one key of a keyed source.
     *
     * Reads a fixed key out of a signal of KeyedElements. The key need not be
     * present at every update: while it is absent the signal holds the last
     * value it saw, so it always has a value to report, and reports no change
     * until the key returns.
     *
     * The latched value is per-context state, looked up by a UniqueId carried
     * in the description, so every instantiation of one description into one
     * context latches together while two contexts stay independent.
     *
     * A change to any element of the source is reported as a change of this
     * signal, whether or not the picked element moved. Apply `tryCheck()` to
     * suppress the repeats for an element type that can be compared.
     */
    template <typename TKey, typename TValue, typename TSignal>
    class Pick
    {
    public:
        struct ContextDataType
        {
            explicit ContextDataType(TValue value) :
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
         * Instantiations of one description are the only owners of their
         * latch, so a context holding no live instantiation has nothing to
         * latch and the key must be present in the source. The caller owes
         * that: build a pick when its key appears, and drop it no later than
         * the update that observes the key leaving.
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
                auto keyed = sig_.evaluate(context, signalData)
                    .template get<0>();

                auto i = keyed->find(key_);
                if (i == keyed->end())
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

            auto keyed = sig_.evaluate(context, data.signalData)
                .template get<0>();

            auto i = keyed->find(key_);
            if (i == keyed->end())
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
    auto pick(Signal<TStorage, KeyedElements<TKey, TValue>> source,
            IdentityT<TKey> key)
    {
        using Storage = SignalStorageType<TStorage,
              KeyedElements<TKey, TValue>>;

        return wrap(Pick<TKey, TValue, Storage>(
                    std::move(source).unwrap(),
                    std::move(key),
                    makeUniqueId()
                    ));
    }

    /** @brief Shares a signal of a vector as a signal of KeyedElements.
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
                    auto keyed = std::make_shared<std::map<TKey, T>>();

                    for (auto const& item : items)
                    {
                        auto inserted = keyed->insert({ keyFn(item), item });
                        assert(inserted.second);
                        (void)inserted;
                    }

                    return KeyedElements<TKey, T>(std::move(keyed));
                })
            .share();
    }
} // namespace bq::signal::detail
