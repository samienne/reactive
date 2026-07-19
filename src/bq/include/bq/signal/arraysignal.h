#pragma once

#include "combine.h"
#include "constant.h"
#include "join.h"
#include "merge.h"
#include "signal.h"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

namespace bq::signal
{
    template <typename T>
    class ArraySignal;

    namespace detail
    {
        /** @brief The identity space of one ArraySignal.
         *
         * Constructed together with the ArraySignal that owns it. Ids are
         * unique within one space and mean nothing outside it, and composing
         * two arrays keeps their spaces distinct — which is what makes their
         * ids distinct without any shared counter.
         */
        class ArrayIdSpace
        {
        public:
            uint64_t allocate()
            {
                return next_.fetch_add(1, std::memory_order_relaxed);
            }

        private:
            std::atomic<uint64_t> next_{ 1 };
        };
    } // namespace detail

    /** @brief The identity of one element of one ArraySignal.
     *
     * Ids never reach user code: they are minted by an ArraySignal and matched
     * within it. An id is equality-comparable and orderable so that it can be
     * used as a map key; nothing else about it is meaningful, and in particular
     * its ordering says nothing about element order.
     */
    class ArrayId
    {
    public:
        ArrayId() = default;

        ArrayId(std::shared_ptr<detail::ArrayIdSpace> space, uint64_t index) :
            space_(std::move(space)),
            index_(index)
        {
        }

        bool operator==(ArrayId const& rhs) const
        {
            return index_ == rhs.index_ && space_ == rhs.space_;
        }

        bool operator!=(ArrayId const& rhs) const
        {
            return !(*this == rhs);
        }

        bool operator<(ArrayId const& rhs) const
        {
            if (space_ != rhs.space_)
                return std::less<void const*>()(space_.get(), rhs.space_.get());

            return index_ < rhs.index_;
        }

    private:
        std::shared_ptr<detail::ArrayIdSpace> space_;
        uint64_t index_ = 0;
    };

    namespace detail
    {
        /** @brief One realised element: its identity and its value over time.
         *
         * An element that survives a membership change keeps both — in
         * particular the very same value signal object, so anything an operator
         * built on that signal keeps its state.
         *
         * Two elements are equal when they have the same identity; the value is
         * not part of the comparison. That makes an element sequence
         * equality-comparable, so `tryCheck()` on it suppresses a change
         * notification exactly when membership is unchanged.
         */
        template <typename T>
        struct ArrayElement
        {
            ArrayId id;
            AnySignal<T> value;

            bool operator==(ArrayElement const& rhs) const
            {
                return id == rhs.id;
            }

            bool operator!=(ArrayElement const& rhs) const
            {
                return id != rhs.id;
            }
        };

        template <typename T>
        using ArrayElements = std::vector<ArrayElement<T>>;

        /** @brief Applies a function once per identity and caches the result.
         *
         * This is the shared primitive under `map`, `forEach`, `extract` and
         * `scatter`. The cache is keyed by identity rather than by position, so
         * a sibling insertion or a reorder does not disturb it. An entry is
         * evicted when its id leaves the array, and a re-appearing id is a new
         * item that is built again.
         *
         * The cache lives with the operator rather than in a SignalContext,
         * because everything it holds is an immutable description — a signal,
         * or a value the delegate built. It is mutex-guarded because a signal
         * may be evaluated from any thread.
         */
        template <typename T, typename U, typename TFunc>
        class OncePerId
        {
        public:
            OncePerId(TFunc func) :
                func_(std::move(func))
            {
            }

            ArrayElements<U> apply(ArrayElements<T> const& elements)
            {
                std::lock_guard<std::mutex> lock(mutex_);

                ArrayElements<U> result;
                result.reserve(elements.size());

                std::set<ArrayId> live;
                for (auto const& element : elements)
                {
                    auto i = cache_.find(element.id);
                    if (i == cache_.end())
                    {
                        i = cache_.emplace(element.id,
                                func_(element.id, element.value)).first;
                    }

                    live.insert(element.id);
                    result.push_back({ element.id, i->second });
                }

                for (auto i = cache_.begin(); i != cache_.end();)
                    i = live.count(i->first) ? std::next(i) : cache_.erase(i);

                return result;
            }

        private:
            std::mutex mutex_;
            std::map<ArrayId, AnySignal<U>> cache_;
            TFunc func_;
        };

        /** @brief Runs `func` once per identity over an element signal. */
        template <typename U, typename T, typename TFunc>
        AnySignal<ArrayElements<U>> mapElementsOnce(
                AnySignal<ArrayElements<T>> elements, TFunc func)
        {
            auto once = std::make_shared<OncePerId<T, U, TFunc>>(
                    std::move(func));

            return elements.map([once](ArrayElements<T> const& es)
                {
                    return once->apply(es);
                });
        }

        /** @brief Fans the element values in, discarding identity. */
        template <typename T>
        AnySignal<std::vector<T>> flattenValues(
                AnySignal<ArrayElements<T>> elements)
        {
            return elements
                .map([](ArrayElements<T> const& es)
                    {
                        std::vector<AnySignal<T>> sigs;
                        sigs.reserve(es.size());
                        for (auto const& element : es)
                            sigs.push_back(element.value);

                        return AnySignal<std::vector<T>>(
                                combine(std::move(sigs)));
                    })
                .join();
        }

        /** @brief A source vector re-expressed as keyed entries.
         *
         * `order` is the source order; `byEntry` is the lookup a per-item
         * signal uses to find its own value. The entry is the caller's key
         * paired with its occurrence index, which is zero for every key unless
         * the caller supplied duplicates.
         */
        template <typename TKey, typename T>
        struct KeyedSource
        {
            using Entry = std::pair<TKey, size_t>;

            std::vector<Entry> order;
            std::map<Entry, T> byEntry;
        };

        template <typename T>
        struct IsArraySignal : std::false_type
        {
        };

        template <typename T>
        struct IsArraySignal<ArraySignal<T>> : std::true_type
        {
        };

        template <typename T>
        struct IsAnySignal : std::false_type
        {
        };

        template <typename... Ts>
        struct IsAnySignal<AnySignal<Ts...>> : std::true_type
        {
        };
    } // namespace detail

    /** @brief An immutable description of a list whose contents and membership
     * may both change over time.
     *
     * An ArraySignal is a value, not a container: it has no mutating
     * operations, and dynamism reaches it from upstream. Each element carries
     * an identity, which is what lets an operator build something once per item
     * and keep it across insertions, removals and reorderings of its siblings.
     *
     * Every accepted source converts to an ArraySignal, so a braced list of
     * them is itself one and nesting needs no special case in a consumer:
     *
     * @code
     * ArraySignal<int> values = { 1, 2, someSignal, otherArray };
     * @endcode
     *
     * `{}` is the empty list, and `{x}` is a one-element list — which exports
     * the same sequence as the single item `x` would.
     *
     * The type is type-erased by construction and has no storage-typed twin;
     * there is deliberately no `AnyArraySignal`.
     */
    template <typename T>
    class ArraySignal
    {
    public:
        using ElementsSignal = AnySignal<detail::ArrayElements<T>>;

        /** @brief Constructs the empty list. */
        ArraySignal() :
            ArraySignal(ElementsSignal(constant(detail::ArrayElements<T>())))
        {
        }

        /** @brief Constructs a list of children, flattened in order. */
        ArraySignal(std::initializer_list<ArraySignal<T>> children) :
            ArraySignal(std::vector<ArraySignal<T>>(children))
        {
        }

        /** @overload */
        ArraySignal(std::vector<ArraySignal<T>> children) :
            ArraySignal(concatElements(std::move(children)))
        {
        }

        /** @brief Constructs a fixed list of constant items. */
        ArraySignal(std::vector<T> values) :
            ArraySignal(constantElements(std::move(values)))
        {
        }

        /** @brief Constructs a single item whose value varies over time. */
        ArraySignal(AnySignal<T> value) :
            ArraySignal(singleElement(std::move(value)))
        {
        }

        /** @brief Constructs a varying list of items.
         *
         * Identity here is positional — item *k* is item *k* for as long as the
         * list is at least that long, and a shorter list evicts the tail. Key
         * the items with `forEach` instead when the list reorders.
         */
        ArraySignal(AnySignal<std::vector<T>> values) :
            ArraySignal(positionalElements(std::move(values)))
        {
        }

        /** @brief Constructs a single constant item. */
        template <typename U, typename = std::enable_if_t<
            std::is_convertible_v<U, T>
            && !detail::IsArraySignal<std::decay_t<U>>::value
            && !detail::IsAnySignal<std::decay_t<U>>::value
            >>
        ArraySignal(U&& value) :
            ArraySignal(singleElement(AnySignal<T>(
                            constant(T(std::forward<U>(value))))))
        {
        }

        /** @brief Transforms every element's value.
         *
         * Value-level: `func` is re-run whenever an element's value changes,
         * inside a graph node that is built once per identity. Do not construct
         * anything stateful here — a widget built in `map` is rebuilt, and
         * loses whatever it was holding, on every value change. Build in
         * `forEach` instead.
         */
        template <typename TFunc>
        auto map(TFunc func) const
        {
            using U = std::decay_t<std::invoke_result_t<TFunc, T const&>>;

            return ArraySignal<U>::fromElements(detail::mapElementsOnce<U>(
                        elements_,
                        [func](ArrayId const&, AnySignal<T> const& value)
                        {
                            return AnySignal<U>(value.map(func).share());
                        }));
        }

        /** @brief Keys the elements and builds one result per key.
         *
         * Identity-level: `delegate` receives the item's value as a signal and
         * is invoked once per key. It is not re-invoked when the item's value
         * changes — the new value arrives through the signal the delegate
         * already holds — so nothing the delegate built is destroyed by a value
         * change. This is what preserves the state of what was built.
         *
         * `keyFn` runs for every item on every change. A changed key is a new
         * item: the old key's result is destroyed and the new key's is built.
         *
         * Duplicate keys assert in debug. In release each occurrence of a
         * repeated key gets its own identity, in order of appearance, so
         * nothing is dropped and nothing churns.
         */
        template <typename TKeyFunc, typename TDelegate>
        auto forEach(TKeyFunc keyFn, TDelegate delegate) const;

        /** @brief The identified elements.
         *
         * An operator's entry point into the array, not part of the
         * caller-facing surface.
         */
        ElementsSignal const& elements() const
        {
            return elements_;
        }

        /** @brief Wraps an element signal an operator produced.
         *
         * Not part of the caller-facing surface.
         */
        static ArraySignal fromElements(ElementsSignal elements)
        {
            return ArraySignal(std::move(elements));
        }

    private:
        explicit ArraySignal(ElementsSignal elements) :
            elements_(std::move(elements).tryCheck().share())
        {
        }

        static ElementsSignal singleElement(AnySignal<T> value)
        {
            auto space = std::make_shared<detail::ArrayIdSpace>();

            detail::ArrayElements<T> elements;
            elements.push_back({ ArrayId(space, space->allocate()),
                    std::move(value) });

            return constant(std::move(elements));
        }

        static ElementsSignal constantElements(std::vector<T> values)
        {
            auto space = std::make_shared<detail::ArrayIdSpace>();

            detail::ArrayElements<T> elements;
            for (auto& value : values)
            {
                elements.push_back({ ArrayId(space, space->allocate()),
                        AnySignal<T>(constant(std::move(value))) });
            }

            return constant(std::move(elements));
        }

        static ElementsSignal positionalElements(
                AnySignal<std::vector<T>> values)
        {
            struct State
            {
                std::mutex mutex;
                std::shared_ptr<detail::ArrayIdSpace> space =
                    std::make_shared<detail::ArrayIdSpace>();
                detail::ArrayElements<T> byPosition;
            };

            auto state = std::make_shared<State>();
            auto source = AnySignal<std::vector<T>>(values.share());

            return source
                .map([](std::vector<T> const& current)
                    {
                        return current.size();
                    })
                .tryCheck()
                .merge(source)
                .map([state, source](size_t size,
                            std::vector<T> const& current)
                    {
                        std::lock_guard<std::mutex> lock(state->mutex);

                        while (state->byPosition.size() < size)
                        {
                            size_t const index = state->byPosition.size();
                            T initial = current[index];

                            auto value = AnySignal<T>(source
                                    .map([index, initial](
                                                std::vector<T> const& v) -> T
                                        {
                                            return index < v.size()
                                                ? v[index] : initial;
                                        })
                                    .share());

                            state->byPosition.push_back({
                                    ArrayId(state->space,
                                            state->space->allocate()),
                                    std::move(value) });
                        }

                        return detail::ArrayElements<T>(
                                state->byPosition.begin(),
                                state->byPosition.begin()
                                + static_cast<ptrdiff_t>(size));
                    });
        }

        static ElementsSignal concatElements(
                std::vector<ArraySignal<T>> children)
        {
            std::vector<AnySignal<detail::ArrayElements<T>>> sigs;
            sigs.reserve(children.size());
            for (auto const& child : children)
                sigs.push_back(child.elements());

            return combine(std::move(sigs))
                .map([](std::vector<detail::ArrayElements<T>> const& groups)
                    {
                        detail::ArrayElements<T> result;
                        for (auto const& group : groups)
                        {
                            result.insert(result.end(), group.begin(),
                                    group.end());
                        }

                        return result;
                    });
        }

        ElementsSignal elements_;
    };

    /** @brief Keys a changing list and builds one result per key.
     *
     * The entry into the ArraySignal domain, and the only operation other than
     * constructing a single item that mints identity. See
     * `ArraySignal::forEach` for the semantics of `keyFn` and `delegate`.
     */
    template <typename T, typename TKeyFunc, typename TDelegate>
    auto forEach(AnySignal<std::vector<T>> source, TKeyFunc keyFn,
            TDelegate delegate)
    {
        using Key = std::decay_t<std::invoke_result_t<TKeyFunc, T const&>>;
        using Keyed = detail::KeyedSource<Key, T>;
        using Entry = typename Keyed::Entry;
        using U = std::decay_t<std::invoke_result_t<TDelegate, AnySignal<T>>>;

        auto keyed = AnySignal<Keyed>(source
                .map([keyFn](std::vector<T> const& values)
                    {
                        Keyed result;
                        std::map<Key, size_t> seen;

                        for (auto const& value : values)
                        {
                            Key key = keyFn(value);
                            size_t const occurrence = seen[key]++;
                            assert(occurrence == 0
                                    && "forEach: duplicate key");

                            Entry entry(std::move(key), occurrence);
                            result.byEntry.emplace(entry, value);
                            result.order.push_back(std::move(entry));
                        }

                        return result;
                    })
                .share());

        struct State
        {
            std::mutex mutex;
            std::shared_ptr<detail::ArrayIdSpace> space =
                std::make_shared<detail::ArrayIdSpace>();
            std::map<Entry, detail::ArrayElement<T>> items;
        };

        auto state = std::make_shared<State>();

        auto elements = AnySignal<detail::ArrayElements<T>>(
                keyed.map([state, keyed](Keyed const& current)
                    {
                        std::lock_guard<std::mutex> lock(state->mutex);

                        detail::ArrayElements<T> result;
                        result.reserve(current.order.size());

                        for (auto const& entry : current.order)
                        {
                            auto i = state->items.find(entry);
                            if (i == state->items.end())
                            {
                                T initial = current.byEntry.at(entry);

                                auto value = AnySignal<T>(keyed
                                        .map([entry, initial](
                                                    Keyed const& now) -> T
                                            {
                                                auto j = now.byEntry.find(
                                                        entry);
                                                return j == now.byEntry.end()
                                                    ? initial : j->second;
                                            })
                                        .share());

                                i = state->items.emplace(entry,
                                        detail::ArrayElement<T>{
                                            ArrayId(state->space,
                                                    state->space->allocate()),
                                            std::move(value) }).first;
                            }

                            result.push_back(i->second);
                        }

                        for (auto i = state->items.begin();
                                i != state->items.end();)
                        {
                            i = current.byEntry.count(i->first)
                                ? std::next(i) : state->items.erase(i);
                        }

                        return result;
                    }));

        return ArraySignal<U>::fromElements(detail::mapElementsOnce<U>(
                    std::move(elements),
                    [delegate](ArrayId const&, AnySignal<T> const& value)
                    {
                        return AnySignal<U>(constant(delegate(value)));
                    }));
    }

    /** @overload */
    template <typename T, typename TKeyFunc, typename TDelegate>
    auto forEach(std::vector<T> source, TKeyFunc keyFn, TDelegate delegate)
    {
        return forEach(AnySignal<std::vector<T>>(constant(std::move(source))),
                std::move(keyFn), std::move(delegate));
    }

    template <typename T>
    template <typename TKeyFunc, typename TDelegate>
    auto ArraySignal<T>::forEach(TKeyFunc keyFn, TDelegate delegate) const
    {
        return signal::forEach(detail::flattenValues(elements_),
                std::move(keyFn), std::move(delegate));
    }

    /** @brief Concatenates arrays, preserving order and every identity.
     *
     * The children keep their own identity spaces, so their ids stay distinct
     * without being rewritten. With the empty array as its identity element
     * this is a monoid, which is the same statement as `{a, {b, c}}` and
     * `{a, b, c}` exporting the same list.
     */
    template <typename T>
    ArraySignal<T> concat(std::vector<ArraySignal<T>> arrays)
    {
        return ArraySignal<T>(std::move(arrays));
    }

    /** @overload */
    template <typename T, typename... Ts>
    ArraySignal<T> concat(ArraySignal<T> first, Ts&&... rest)
    {
        return concat(std::vector<ArraySignal<T>>{ std::move(first),
                ArraySignal<T>(std::forward<Ts>(rest))... });
    }
} // namespace bq::signal
