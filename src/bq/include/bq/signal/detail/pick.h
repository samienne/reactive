#pragma once

#include "bq/signal/signal.h"

#include <btl/demangle.h>

#include <cassert>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
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
    struct NonDeduced
    {
        using type = T;
    };

    template <typename T>
    using NonDeducedT = typename NonDeduced<T>::type;

    /** @brief Follows the element stored under one key of a keyed source.
     *
     * Yields no value for an update in which the key is absent from the
     * source. A consumer that requires the element to be there says so with
     * requirePresent().
     *
     * @param source A keyed source, normally the result of shareKeyed().
     * @param key    The key to follow. It is carried by value and is the only
     *               thing that distinguishes one pick from another.
     */
    template <typename TStorage, typename TKey, typename TValue>
    auto pick(Signal<TStorage, KeyedElements<TKey, TValue>> source,
            NonDeducedT<TKey> key)
    {
        return std::move(source).map(
                [key=std::move(key)](KeyedElements<TKey, TValue> const& keyed)
                {
                    auto i = keyed->find(key);
                    if (i == keyed->end())
                        return std::optional<TValue>();

                    return std::optional<TValue>(i->second);
                });
    }

    /** @brief Asserts that a signal has a value, and drops the optional.
     *
     * An empty value is a hard error rather than a state to recover from: the
     * caller has said the value is there by construction, so its absence is a
     * bug in whatever was supposed to keep it there. For a pick that means the
     * caller owes the key's presence — build a pick when its key appears, and
     * drop it no later than the update that observes the key leaving.
     *
     * @throws std::runtime_error if the signal has no value.
     */
    template <typename TStorage, typename T>
    auto requirePresent(Signal<TStorage, std::optional<T>> source)
    {
        return std::move(source).map([](std::optional<T> const& value) -> T
                {
                    if (!value)
                        throw std::runtime_error("requirePresent: no value of "
                                + btl::demangle(typeid(T).name()));

                    return *value;
                });
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
