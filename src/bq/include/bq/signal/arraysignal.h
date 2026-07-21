#pragma once

#include "detail/pick.h"

#include "combine.h"
#include "constant.h"
#include "datacontext.h"
#include "frameinfo.h"
#include "join.h"
#include "signal.h"
#include "signalresult.h"
#include "signaltraits.h"
#include "updateresult.h"

#include <btl/connection.h>
#include <btl/copywrapper.h>
#include <btl/uniqueid.h>

#include <functional>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace bq::signal
{
    template <typename T>
    class ArraySignal;

    namespace detail
    {
        /** @brief The identity of one element of one array.
         *
         * An id never reaches user code: it is minted by an array's node, in
         * that node's per-context state, and is only ever matched within the
         * node that minted it. It is orderable so that it can key a map;
         * nothing else about it means anything, and in particular its order
         * says nothing about the order of the elements.
         */
        class ArrayId
        {
        public:
            explicit ArrayId(btl::UniqueId id) :
                id_(id)
            {
            }

            bool operator==(ArrayId const& rhs) const
            {
                return id_ == rhs.id_;
            }

            bool operator!=(ArrayId const& rhs) const
            {
                return id_ != rhs.id_;
            }

            bool operator<(ArrayId const& rhs) const
            {
                return id_ < rhs.id_;
            }

        private:
            btl::UniqueId id_;
        };

        inline ArrayId makeArrayId()
        {
            return ArrayId(makeUniqueId());
        }

        /** @brief One element of an array: its identity and what was built
         * for it.
         *
         * Equality is identity alone, so comparing two element sequences asks
         * whether the membership changed and nothing more. The value cannot
         * differ at a fixed identity: it is built once when the identity
         * appears and kept until the identity leaves.
         */
        template <typename T>
        struct ArrayElement
        {
            ArrayId id;
            T value;

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

        template <typename T>
        struct IsArraySignal : std::false_type
        {
        };

        template <typename T>
        struct IsArraySignal<ArraySignal<T>> : std::true_type
        {
        };

        /** @brief Builds one value per key and keeps it while the key is
         * there.
         *
         * The key to id map and the built values live in this node's DataType,
         * which an array always holds behind a share(). SharedControl keys that
         * data by a UniqueId carried in the description, so the table is
         * created once per DataContext: two contexts over one description build
         * two independent sets of values, and two consumers within one context
         * find the one table.
         *
         * `keyFunc`, `buildFunc` and `idFunc` are invoked as const functions.
         * They live in the description, which every context shares, so state in
         * them would be state outside any context.
         *
         * @throws std::runtime_error if two elements of one update share a key.
         */
        template <typename TSource, typename TKeyFunc, typename TBuildFunc,
                 typename TIdFunc>
        class ArrayOnce
        {
        public:
            using Storage = typename AnySignal<std::vector<TSource>>::
                StorageType;

            using Key = std::decay_t<std::invoke_result_t<
                TKeyFunc const&, TSource const&>>;

            using Value = std::decay_t<std::invoke_result_t<
                TBuildFunc const&, Key const&, TSource const&>>;

            using Elements = ArrayElements<Value>;

            struct DataType
            {
                typename Storage::DataType innerData;
                std::map<Key, ArrayElement<Value>> built;
                Elements elements;
            };

            ArrayOnce(Storage sig, TKeyFunc keyFunc, TBuildFunc buildFunc,
                    TIdFunc idFunc) :
                sig_(std::move(sig)),
                keyFunc_(std::move(keyFunc)),
                buildFunc_(std::move(buildFunc)),
                idFunc_(std::move(idFunc))
            {
            }

            DataType initialize(DataContext& context,
                    FrameInfo const& frame) const
            {
                DataType data { sig_.initialize(context, frame), {}, {} };

                refresh(data, sig_.evaluate(context, data.innerData)
                        .template get<0>());

                return data;
            }

            SignalResult<Elements const&> evaluate(DataContext&,
                    DataType const& data) const
            {
                return SignalResult<Elements const&>(data.elements);
            }

            UpdateResult update(DataContext& context, DataType& data,
                    FrameInfo const& frame)
            {
                UpdateResult r = sig_.update(context, data.innerData, frame);

                if (r.didChange)
                {
                    r.didChange = refresh(data,
                            sig_.evaluate(context, data.innerData)
                            .template get<0>());
                }

                return r;
            }

            btl::connection observe(DataContext& context, DataType& data,
                    std::function<void()> callback)
            {
                return sig_.observe(context, data.innerData,
                        std::move(callback));
            }

        private:
            // Returns whether the membership changed, which is the whole of
            // what an update can change: a value is built once per identity.
            bool refresh(DataType& data,
                    std::vector<TSource> const& source) const
            {
                std::map<Key, ArrayElement<Value>> built;
                Elements elements;
                elements.reserve(source.size());

                for (auto const& item : source)
                {
                    Key key = (*keyFunc_)(item);

                    if (built.count(key))
                        throw std::runtime_error("ArraySignal: duplicate key");

                    auto previous = data.built.find(key);
                    ArrayElement<Value> element =
                        previous != data.built.end()
                        ? previous->second
                        : ArrayElement<Value>{ (*idFunc_)(key),
                            (*buildFunc_)(key, item) };

                    built.insert({ key, element });
                    elements.push_back(std::move(element));
                }

                bool const changed = elements != data.elements;

                // Assigning the new table here is what retires a departed
                // key's value, in the same update that observed it leave.
                data.built = std::move(built);
                data.elements = std::move(elements);

                return changed;
            }

            Storage sig_;
            mutable btl::CopyWrapper<TKeyFunc> keyFunc_;
            mutable btl::CopyWrapper<TBuildFunc> buildFunc_;
            mutable btl::CopyWrapper<TIdFunc> idFunc_;
        };

        template <typename TSource, typename TKeyFunc, typename TBuildFunc,
                 typename TIdFunc>
        auto makeArrayOnce(AnySignal<std::vector<TSource>> source,
                TKeyFunc keyFunc, TBuildFunc buildFunc, TIdFunc idFunc)
        {
            return wrap(ArrayOnce<TSource, TKeyFunc, TBuildFunc, TIdFunc>(
                        std::move(source).unwrap(),
                        std::move(keyFunc),
                        std::move(buildFunc),
                        std::move(idFunc)));
        }

        /** @brief One element whose value comes from a signal.
         *
         * A new value is a new identity. Nothing built this value from an
         * identity that outlives the change, so whatever an operator built
         * once per identity has to be built again — which is the rebuild an
         * array of a varying single item is defined to cost. Only this element
         * is disturbed.
         */
        template <typename T>
        class ArraySingle
        {
        public:
            using Storage = typename AnySignal<T>::StorageType;
            using Elements = ArrayElements<T>;

            struct DataType
            {
                typename Storage::DataType innerData;
                Elements elements;
            };

            ArraySingle(Storage sig) :
                sig_(std::move(sig))
            {
            }

            DataType initialize(DataContext& context,
                    FrameInfo const& frame) const
            {
                DataType data { sig_.initialize(context, frame), {} };

                setValue(data, sig_.evaluate(context, data.innerData)
                        .template get<0>());

                return data;
            }

            SignalResult<Elements const&> evaluate(DataContext&,
                    DataType const& data) const
            {
                return SignalResult<Elements const&>(data.elements);
            }

            UpdateResult update(DataContext& context, DataType& data,
                    FrameInfo const& frame)
            {
                UpdateResult r = sig_.update(context, data.innerData, frame);

                if (r.didChange)
                {
                    setValue(data, sig_.evaluate(context, data.innerData)
                            .template get<0>());
                }

                return r;
            }

            btl::connection observe(DataContext& context, DataType& data,
                    std::function<void()> callback)
            {
                return sig_.observe(context, data.innerData,
                        std::move(callback));
            }

        private:
            static void setValue(DataType& data, T const& value)
            {
                data.elements.clear();
                data.elements.push_back({ makeArrayId(), value });
            }

            Storage sig_;
        };
    } // namespace detail

    /** @brief An immutable description of a list whose contents and membership
     * may both change over time.
     *
     * An array is a value, not a container: it has no mutating operations, and
     * dynamism reaches it from upstream. Each element carries an identity,
     * which is what lets an operator build something once per item and keep it
     * across insertions, removals and reorderings of its siblings.
     *
     * Every accepted source converts to an array, so a braced list of them is
     * itself one and nesting needs no special case in a consumer:
     *
     * @code
     * ArraySignal<int> values = { 1, 2, someSignal, otherArray };
     * @endcode
     *
     * `{}` is the empty list, and `{x}` is a one-element list — which exports
     * the same sequence as the single item `x` would.
     *
     * A list whose membership varies is not a constructor: it is forEach(),
     * which asks for the key that says which item is which. There is no
     * constructor from AnySignal<std::vector<T>>, because keying such a list by
     * position is not identity preservation.
     *
     * The type is type-erased by construction and has no storage-typed twin;
     * there is deliberately no `AnyArraySignal`.
     *
     * An array is shared: it holds its elements behind a share(), so two
     * consumers of one array within one SignalContext see one set of built
     * values and one evaluation per pass. Two SignalContexts over one array
     * share nothing at all.
     */
    template <typename T>
    class ArraySignal
    {
    public:
        using Elements = detail::ArrayElements<T>;
        using ElementsSignal = AnySignal<Elements>;

        /** @brief Constructs the empty list. */
        ArraySignal() :
            ArraySignal(ElementsSignal(constant(Elements())))
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

        /** @brief Constructs a single item whose value varies over time.
         *
         * A changed value rebuilds this element, and only this element.
         */
        ArraySignal(AnySignal<T> value) :
            ArraySignal(ElementsSignal(wrap(detail::ArraySingle<T>(
                                std::move(value).unwrap()))))
        {
        }

        /** @brief Constructs a single constant item. */
        template <typename U, typename = std::enable_if_t<
            std::is_convertible_v<U, T>
            && !std::is_convertible_v<U, AnySignal<T>>
            && !detail::IsArraySignal<std::decay_t<U>>::value
            >>
        ArraySignal(U&& value) :
            ArraySignal(AnySignal<T>(constant(T(std::forward<U>(value)))))
        {
        }

        /** @brief Builds one value per element, once per identity.
         *
         * `func` is invoked when an identity appears and its result is kept
         * until the identity leaves, so it is not re-invoked because a value
         * changed — a value change reaches what was built through the signals
         * that were wired into it.
         *
         * That still makes this the wrong place to construct anything holding
         * state that must survive: an element of a value-dynamic array gets a
         * new identity whenever its value changes, and `func` runs again for
         * it. Transform data here; build in forEach(), where the identity is
         * keyed and outlives the value.
         */
        template <typename TFunc>
        auto map(TFunc func) const
        {
            using Element = detail::ArrayElement<T>;

            auto build = [func=std::move(func)](detail::ArrayId const&,
                    Element const& element)
                {
                    return func(element.value);
                };

            using V = std::decay_t<std::invoke_result_t<decltype(build) const&,
                  detail::ArrayId const&, Element const&>>;

            return ArraySignal<V>::fromElements(detail::makeArrayOnce(
                        elements_,
                        [](Element const& element)
                        {
                            return element.id;
                        },
                        std::move(build),
                        [](detail::ArrayId const& id)
                        {
                            return id;
                        }));
        }

        /** @brief The identified elements.
         *
         * An operator's entry into the array, not part of the caller-facing
         * surface.
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
            elements_(elements.share())
        {
        }

        static ElementsSignal concatElements(
                std::vector<ArraySignal<T>> children)
        {
            std::vector<ElementsSignal> sigs;
            sigs.reserve(children.size());
            for (auto const& child : children)
                sigs.push_back(child.elements());

            return combine(std::move(sigs))
                .map([](std::vector<Elements> const& groups)
                    {
                        Elements result;
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

    /** @brief Keys a changing list and builds one value per key.
     *
     * The entry into the array domain, and the only operator that mints
     * identity from a key. `delegate` receives the item's value as a signal and
     * is invoked once per key per context. It is not re-invoked when the item's
     * value changes — the new value arrives through the signal the delegate
     * already holds — so nothing the delegate built is destroyed by a value
     * change. That is what preserves the state of what was built.
     *
     * Eviction is strict. When a key leaves, its identity is dropped and what
     * the delegate built for it is destroyed in that same update; a key that
     * leaves and returns is a new item, exactly as a changed key is.
     *
     * `keyFn` and `delegate` are invoked as const functions, and run for every
     * item on every change and once per key respectively. Keys must be unique
     * within this call and need not be unique anywhere else.
     *
     * @throws std::runtime_error if two items share a key.
     */
    template <typename T, typename TKeyFunc, typename TDelegate>
    auto forEach(AnySignal<std::vector<T>> source, TKeyFunc keyFn,
            TDelegate delegate)
    {
        using Key = std::decay_t<std::invoke_result_t<
            TKeyFunc const&, T const&>>;

        using U = std::decay_t<std::invoke_result_t<
            TDelegate const&, AnySignal<T>>>;

        auto shared = AnySignal<std::vector<T>>(source.share());
        auto keyed = detail::shareKeyed(shared, keyFn);

        auto build = [keyed, delegate=std::move(delegate)](Key const& key,
                T const&)
            {
                return delegate(AnySignal<T>(detail::requirePresent(
                                detail::pick(keyed, key),
                                "an element of forEach")));
            };

        return ArraySignal<U>::fromElements(detail::makeArrayOnce(
                    std::move(shared),
                    std::move(keyFn),
                    std::move(build),
                    [](Key const&)
                    {
                        return detail::makeArrayId();
                    }));
    }

    /** @overload */
    template <typename T, typename TKeyFunc, typename TDelegate>
    auto forEach(std::vector<T> source, TKeyFunc keyFn, TDelegate delegate)
    {
        return forEach(AnySignal<std::vector<T>>(constant(std::move(source))),
                std::move(keyFn), std::move(delegate));
    }

    /** @brief Concatenates arrays, preserving order and every identity.
     *
     * Identities are distinct between arrays by construction, so nothing is
     * re-keyed. With the empty array as its identity element this is a monoid,
     * which is the same statement as `{a, {b, c}}` and `{a, b, c}` exporting
     * the same list.
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

    /** @brief Fans the elements in, in membership order.
     *
     * The only exit from the array domain, and the reason the element type has
     * to be a signal: an array of anything else has nothing to fan in. Map it
     * to something joinable first.
     *
     * Each element's signal is shared once per identity, so a membership change
     * rebuilds the fan-in without disturbing what a surviving element's signal
     * has accumulated.
     */
    template <typename X>
    AnySignal<std::vector<X>> join(ArraySignal<AnySignal<X>> array)
    {
        auto shared = array.map([](AnySignal<X> const& sig)
            {
                return AnySignal<X>(sig.share());
            });

        return shared.elements()
            .map([](detail::ArrayElements<AnySignal<X>> const& elements)
                {
                    std::vector<AnySignal<X>> sigs;
                    sigs.reserve(elements.size());
                    for (auto const& element : elements)
                        sigs.push_back(element.value);

                    return AnySignal<std::vector<X>>(combine(std::move(sigs)));
                })
            .join();
    }
} // namespace bq::signal
