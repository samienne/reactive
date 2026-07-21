#pragma once

#include "combine.h"
#include "constant.h"
#include "datacontext.h"
#include "detail/pick.h"
#include "frameinfo.h"
#include "merge.h"
#include "signal.h"
#include "signalresult.h"
#include "signaltraits.h"
#include "updateresult.h"

#include <btl/connection.h>
#include <btl/copywrapper.h>
#include <btl/uniqueid.h>

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
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
         * A distinct type rather than a bare btl::UniqueId because it is drawn
         * from the same counter as the ids that key a DataContext and means
         * something different: this one names an entry in an array node's own
         * table. Were they one type, looking an element up in the DataContext —
         * or the reverse — would compile and quietly find nothing.
         *
         * An id never reaches user code: it is minted where the element is,
         * and is only ever matched within the node that minted it. It is
         * orderable so that it can key a map; nothing else about it means
         * anything, and in particular its order says nothing about the order of
         * the elements.
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

        /** @brief Whether `U` is accepted as a single constant item of `T`.
         *
         * An array has its own constructors, so an unconstrained forwarding
         * constructor would swallow it — and it would be a better match than
         * the copy constructor for a non-const array lvalue besides.
         */
        template <typename U, typename T>
        constexpr bool isPlainItem =
            std::is_convertible_v<U, T>
            && !IsArraySignal<std::decay_t<U>>::value;

        /** @brief Whether forEach() accepts this key function.
         *
         * Constrained at the call so that a callable of the wrong shape is
         * reported where it was written rather than as a failure deep inside
         * the node. Only invocability can be constrained, not the result: that
         * is what the key is deduced *from*, so there is nothing to give
         * is_invocable_r to compare against.
         */
        template <typename TKeyFunc, typename T>
        constexpr bool isForEachKeyFunc =
            std::is_invocable_v<TKeyFunc const&, T const&>;

        /** @brief Whether `TDelegate` is forEach()'s keyed delegate form.
         *
         * The key is a prvalue here because that is how it is handed over: a
         * delegate that keeps it keeps a copy of its own, and one that binds a
         * reference to it is doing what binding a reference to any parameter
         * does.
         */
        template <typename TDelegate, typename TKey, typename T>
        constexpr bool isKeyedForEachDelegate =
            std::is_invocable_v<TDelegate const&, TKey, AnySignal<T>>;

        /** @brief Whether `TDelegate` is forEach()'s plain delegate form. */
        template <typename TDelegate, typename T>
        constexpr bool isPlainForEachDelegate =
            std::is_invocable_v<TDelegate const&, AnySignal<T>>;

        /** @brief Invokes a forEach() delegate in whichever form it takes.
         *
         * A delegate that accepts both — a generic lambda does — is given
         * the key. It is strictly more to work with, and one that names no
         * parameter for it is unaffected.
         */
        template <typename TDelegate, typename TKey, typename T>
        auto invokeForEachDelegate(TDelegate const& delegate, TKey key,
                AnySignal<T> value)
        {
            if constexpr (isKeyedForEachDelegate<TDelegate, TKey, T>)
                return delegate(std::move(key), std::move(value));
            else
                return delegate(std::move(value));
        }

        /** @brief What a forEach() delegate builds, in whichever form it
         * takes.
         *
         * Only well formed for a delegate that is one of the two, which is
         * what forEach() checks before naming it.
         */
        template <typename TDelegate, typename TKey, typename T>
        using ForEachResult = std::decay_t<decltype(invokeForEachDelegate(
                    std::declval<TDelegate const&>(),
                    std::declval<TKey>(),
                    std::declval<AnySignal<T>>()))>;

        /** @brief Whether scatter() accepts this delegate. */
        template <typename TFunc, typename T, typename W>
        constexpr bool isScatterCallable =
            std::is_invocable_v<TFunc const&, T const&, AnySignal<W>>;

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
         * `keyFunc` and `buildFunc` are invoked as const functions. They live
         * in the description, which every context shares, so state in them
         * would be state outside any context. The built value is copied into
         * the exported element sequence, so it must be copy-constructible.
         *
         * @throws std::runtime_error if two elements of one update share a key.
         *         Thrown from a refresh, this discards whatever was built for
         *         the keys already visited, so a build with side effects that
         *         need undoing does not belong here.
         */
        template <typename TSource, typename TKeyFunc, typename TBuildFunc>
        class ArrayOnce
        {
        public:
            using Storage =
                typename AnySignal<std::vector<TSource>>::StorageType;

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

            ArrayOnce(Storage sig, TKeyFunc keyFunc, TBuildFunc buildFunc) :
                sig_(std::move(sig)),
                keyFunc_(std::move(keyFunc)),
                buildFunc_(std::move(buildFunc))
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
                        : ArrayElement<Value>{ makeArrayId(),
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
        };

        template <typename TSource, typename TKeyFunc, typename TBuildFunc>
        auto makeArrayOnce(AnySignal<std::vector<TSource>> source,
                TKeyFunc keyFunc, TBuildFunc buildFunc)
        {
            return wrap(ArrayOnce<TSource, TKeyFunc, TBuildFunc>(
                        std::move(source).unwrap(),
                        std::move(keyFunc),
                        std::move(buildFunc)));
        }

        /** @brief One element that never changes.
         *
         * A constant needs no node: the identity is minted here, once, and the
         * element is then an ordinary constant signal. It is the one identity
         * the description carries, which is harmless because it names nothing
         * outside the per-context tables that match it — two contexts over one
         * constant element look their own values up under the same id and never
         * meet.
         */
        template <typename T>
        AnySignal<ArrayElements<T>> constantElement(T value)
        {
            return constant(ArrayElements<T> {
                    { makeArrayId(), std::move(value) } });
        }

        /** @brief Attaches a positional aggregate to the identities it is
         * aligned with, so that each entry can be followed by identity rather
         * than by position.
         *
         * @throws std::runtime_error if the aggregate does not have one entry
         *         per element, or if one identity appears twice — which the
         *         node keyed by identity rejects first, so reaching it here
         *         means that order has changed.
         */
        template <typename T, typename W>
        KeyedElements<ArrayId, W> keyByIdentity(
                ArrayElements<T> const& elements,
                std::vector<W> const& aggregate)
        {
            if (elements.size() != aggregate.size())
            {
                throw std::runtime_error("scatter: aggregate size "
                        + std::to_string(aggregate.size())
                        + " does not match membership size "
                        + std::to_string(elements.size()));
            }

            auto keyed = std::make_shared<std::map<ArrayId, W>>();

            for (std::size_t i = 0; i != elements.size(); ++i)
            {
                if (!keyed->insert({ elements[i].id, aggregate[i] }).second)
                {
                    throw std::runtime_error("scatter: one identity appears "
                            "twice, which means an array was concatenated "
                            "with itself");
                }
            }

            return KeyedElements<ArrayId, W>(std::move(keyed));
        }

        /** @brief Fans an array of signals in, keeping each identity's state.
         *
         * A membership change would make the generic Join rebuild and
         * re-initialize its whole inner signal, which starts every surviving
         * element over. This node keeps one inner data **per identity**
         * instead: an update carries a survivor's data across untouched,
         * initializes only what arrived, and releases only what left.
         *
         * That is what makes a surviving element's state safe from its
         * neighbours, and it is a property of the node rather than of anything
         * the elements do — nothing here needs the elements to be shared.
         *
         * observe() connects the elements that are there when it is called and
         * is **not** rewired by a later membership change: an element that
         * arrives afterwards is not observed, and one that leaves stays in the
         * returned connection. Nothing in the toolkit observes a signal today.
         *
         * @throws std::runtime_error if one identity appears twice, which means
         *         an array was concatenated with itself.
         */
        template <typename X>
        class ArrayJoin
        {
        public:
            using Elements = ArrayElements<AnySignal<X>>;
            using Storage = typename AnySignal<Elements>::StorageType;
            using InnerData = SignalDataTypeT<AnySignal<X>>;

            struct DataType
            {
                typename Storage::DataType innerData;
                Elements elements;
                std::map<ArrayId, InnerData> datas;
            };

            ArrayJoin(Storage sig) :
                sig_(std::move(sig))
            {
            }

            DataType initialize(DataContext& context,
                    FrameInfo const& frame) const
            {
                DataType data { sig_.initialize(context, frame), {}, {} };

                data.elements = sig_.evaluate(context, data.innerData)
                    .template get<0>();

                requireDistinct(data.elements);

                for (auto const& element : data.elements)
                {
                    data.datas.emplace(element.id,
                            element.value.unwrap().initialize(context, frame));
                }

                return data;
            }

            SignalResult<std::vector<X>> evaluate(DataContext& context,
                    DataType const& data) const
            {
                std::vector<X> result;
                result.reserve(data.elements.size());

                for (auto const& element : data.elements)
                {
                    result.push_back(element.value.unwrap().evaluate(context,
                                data.datas.at(element.id)).template get<0>());
                }

                return SignalResult<std::vector<X>>(std::move(result));
            }

            UpdateResult update(DataContext& context, DataType& data,
                    FrameInfo const& frame)
            {
                UpdateResult r = sig_.update(context, data.innerData, frame);

                std::set<ArrayId> arrived;
                if (r.didChange)
                    arrived = reconcile(context, data, frame);

                for (auto& element : data.elements)
                {
                    // An arrival was initialized against this very frame, so
                    // updating it too would advance it twice — for anything
                    // that folds, that is a wrong value rather than wasted
                    // work.
                    if (arrived.count(element.id))
                        continue;

                    r = r + element.value.unwrap().update(context,
                            data.datas.at(element.id), frame);
                }

                // Nothing collected an arrival's own request to be driven
                // again, so ask for the next frame on its behalf rather than
                // let a newly built animation sit still.
                if (!arrived.empty())
                    r.nextUpdate = min(r.nextUpdate, signal_time_t(0));

                return r;
            }

            btl::connection observe(DataContext& context, DataType& data,
                    std::function<void()> callback)
            {
                btl::connection c = sig_.observe(context, data.innerData,
                        callback);

                for (auto& element : data.elements)
                {
                    c += element.value.unwrap().observe(context,
                            data.datas.at(element.id), callback);
                }

                return c;
            }

        private:
            // Returns the identities that arrived, which the caller owes an
            // update this frame no longer.
            std::set<ArrayId> reconcile(DataContext& context, DataType& data,
                    FrameInfo const& frame) const
            {
                Elements elements = sig_.evaluate(context, data.innerData)
                    .template get<0>();

                requireDistinct(elements);

                // Arrivals are initialized into their own storage first, so
                // nothing in `data` has moved if one of them throws.
                std::map<ArrayId, InnerData> datas;
                std::set<ArrayId> arrived;

                for (auto const& element : elements)
                {
                    if (data.datas.count(element.id))
                        continue;

                    datas.emplace(element.id,
                            element.value.unwrap().initialize(context, frame));

                    arrived.insert(element.id);
                }

                for (auto const& element : elements)
                {
                    auto surviving = data.datas.find(element.id);
                    if (surviving != data.datas.end())
                    {
                        datas.emplace(element.id,
                                std::move(surviving->second));
                    }
                }

                // Assigning last is what releases the departed elements, and
                // it releases them only once everything that arrived has been
                // initialized.
                data.elements = std::move(elements);
                data.datas = std::move(datas);

                return arrived;
            }

            static void requireDistinct(Elements const& elements)
            {
                std::set<ArrayId> seen;

                for (auto const& element : elements)
                {
                    if (!seen.insert(element.id).second)
                    {
                        throw std::runtime_error("ArraySignal: one identity "
                                "appears twice, which means an array was "
                                "concatenated with itself");
                    }
                }
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
     * ArraySignal<int> values = { 1, 2, otherArray };
     * @endcode
     *
     * `{}` is the empty list, and `{x}` is a one-element list — which exports
     * the same sequence as the single item `x` would.
     *
     * **Every constructor is constant.** Anything that varies enters through
     * forEach(), which asks for the key that says which item is which — a
     * varying list, and equally a single varying item. Constructing from a
     * signal would leave the variation nowhere to live but the element itself,
     * which means rebuilding the element, which is the state loss identity
     * exists to prevent. Only construction of a constant item and forEach()
     * mint identity.
     *
     * The type is type-erased by construction and has no storage-typed twin;
     * there is deliberately no `AnyArraySignal`.
     *
     * An array is shared: it holds its elements behind a share(), so two
     * consumers of one array within one SignalContext see **one set of built
     * values** — the delegate and every map() run once between them, which is
     * what identity exists to protect. What is not shared is the elements'
     * instantiated state: each consumer of the exit instantiates the element
     * signals for itself, exactly as instantiating any signal twice in one
     * context does. Share inside the delegate if an element's own state has to
     * be one thing across two consumers.
     *
     * Two SignalContexts over one array share nothing at all.
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

        /** @brief Constructs a single constant item. */
        template <typename U, typename = std::enable_if_t<
            detail::isPlainItem<U, T>
            >>
        ArraySignal(U&& value) :
            ArraySignal(detail::constantElement<T>(
                        T(std::forward<U>(value))))
        {
        }

        /** @brief Builds one value per element, exactly once per identity.
         *
         * `func` is invoked when an identity appears and its result is kept
         * until the identity leaves. It is never re-invoked, because the value
         * it reads cannot change: an element's value is built once when its
         * identity appears, and variation lives inside the signals that value
         * wraps rather than in the value itself.
         *
         * Transform data here; build in forEach(). Both take a function from an
         * element to something else and the type system cannot tell them apart,
         * but only forEach() hands its delegate the signal that carries the
         * item's changing value.
         */
        template <typename TFunc, typename = std::enable_if_t<
            std::is_invocable_v<TFunc const&, T const&>
            >>
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
                        std::move(build)));
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
     * The delegate takes either form:
     *
     * @code
     * (AnySignal<T>) -> U
     * (TKey, AnySignal<T>) -> U
     * @endcode
     *
     * The key comes first, and by value rather than as a signal, because it
     * cannot change: it is the identity, and a delegate handed it as a signal
     * would have to map over a constant to reach it. A delegate that accepts
     * both forms — a generic lambda does — is given the key.
     *
     * Eviction is strict. When a key leaves, its identity is dropped and what
     * the delegate built for it is destroyed in that same update; a key that
     * leaves and returns is a new item, exactly as a changed key is. The
     * update builds the new table before it retires the old one, so a
     * departing item and its replacement briefly coexist — a value holding a
     * resource that only one owner may have cannot be built here.
     *
     * `keyFn` and `delegate` are invoked as const functions, and run for every
     * item on every change and once per key respectively. Keys must be unique
     * within this call and need not be unique anywhere else.
     *
     * `source` is any signal of a `std::vector<T>`, type-erased or not. `T` is
     * the element type the delegate is handed; a source of some other element
     * type goes through an ordinary map() first, which says at the call site
     * what the conversion is.
     *
     * @throws std::runtime_error if two items share a key. Thrown from an
     *         update, this leaves the source advanced and the table not, so
     *         the context should be discarded rather than driven again.
     */
    template <typename TStorage, typename T, typename TKeyFunc,
             typename TDelegate, typename = std::enable_if_t<
                 detail::isForEachKeyFunc<TKeyFunc, T>
                 >>
    auto forEach(Signal<TStorage, std::vector<T>> source, TKeyFunc keyFn,
            TDelegate delegate)
    {
        using Key = std::decay_t<std::invoke_result_t<
            TKeyFunc const&, T const&>>;

        constexpr bool isDelegate =
            detail::isKeyedForEachDelegate<TDelegate, Key, T>
            || detail::isPlainForEachDelegate<TDelegate, T>;

        static_assert(isDelegate, "forEach's delegate must be const callable "
                "as (TKey, AnySignal<T>) or as (AnySignal<T>)");

        // Everything below is discarded when the delegate is neither form, so
        // that the assert above is not followed by a page of failures from
        // inside the node. The call still sees a function returning void.
        if constexpr (isDelegate)
        {
            using U = detail::ForEachResult<TDelegate, Key, T>;

            auto shared = AnySignal<std::vector<T>>(source.share());
            auto keyed = detail::shareKeyed(shared, keyFn);

            auto build = [keyed, delegate=std::move(delegate)](Key const& key,
                    T const&)
                {
                    return detail::invokeForEachDelegate(delegate, key,
                            AnySignal<T>(detail::requirePresent(
                                    detail::pick(keyed, key),
                                    "an element of forEach")));
                };

            return ArraySignal<U>::fromElements(detail::makeArrayOnce(
                        std::move(shared),
                        std::move(keyFn),
                        std::move(build)));
        }
    }

    /** @overload */
    template <typename T, typename TKeyFunc, typename TDelegate,
             typename = std::enable_if_t<
                 detail::isForEachKeyFunc<TKeyFunc, T>
                 >>
    auto forEach(std::vector<T> source, TKeyFunc keyFn, TDelegate delegate)
    {
        return forEach(constant(std::move(source)), std::move(keyFn),
                std::move(delegate));
    }

    /** @brief Concatenates arrays, preserving order and every identity.
     *
     * Separately built arrays have distinct identities by construction, so
     * nothing is re-keyed. With the empty array as its identity element this is
     * a monoid, which is the same statement as `{a, {b, c}}` and `{a, b, c}`
     * exporting the same list.
     *
     * Concatenating an array with *itself* is the one case that is not
     * well-formed: its identities would appear twice, which the next operator
     * over the result reports as a duplicate key. Arrays derived from a common
     * source are fine — every operator that builds an array mints its own
     * identities.
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

    /** @brief Hands every element its own slice of an aggregate computed over
     * all of them.
     *
     * The fan-out half of a container: fan the elements in with join(), compute
     * over all of them at once, and give each one its share back. `func` is
     * invoked once per identity per context, as map()'s function is, and
     * receives the element's value together with a signal carrying that
     * element's entry of every later aggregate. Nothing is rebuilt when the
     * aggregate changes.
     *
     * Every element's slice signal reports a change whenever **any** element's
     * slice does, because they all read one shared source. Suppress the repeats
     * with `.check()` where they matter — anything that folds over a slice
     * accumulates its unchanged value again without it.
     *
     * `aggregate` is **positional**: its entry `i` belongs to element `i` of
     * the current membership. The correspondence is a promise the caller makes
     * and only its arity is checked, on every update in which a slice is read.
     * An aggregate of the right length in the wrong order is accepted and each
     * element is handed the wrong slice, so derive it from this same array by
     * order-preserving steps — map() into join(), then an ordinary map() over
     * the fanned-in vector — and do not route it through anything that sorts,
     * filters, or holds a value from an earlier update.
     *
     * @throws std::runtime_error if the aggregate does not have one entry per
     *         element. Thrown when a slice is read, so it surfaces from
     *         whatever consumes the result rather than from scatter() itself.
     */
    template <typename T, typename TStorage, typename W, typename TFunc,
             typename = std::enable_if_t<
                 detail::isScatterCallable<TFunc, T, W>
                 >>
    auto scatter(ArraySignal<T> array,
            Signal<TStorage, std::vector<W>> aggregate, TFunc func)
    {
        using Element = detail::ArrayElement<T>;
        using Elements = detail::ArrayElements<T>;

        using U = std::decay_t<std::invoke_result_t<
            TFunc const&, T const&, AnySignal<W>>>;

        auto slices = merge(array.elements(), std::move(aggregate))
            .map([](Elements const& elements, std::vector<W> const& entries)
                {
                    return detail::keyByIdentity(elements, entries);
                })
            .share();

        auto build = [slices, func=std::move(func)](detail::ArrayId const& id,
                Element const& element)
            {
                return func(element.value, AnySignal<W>(detail::requirePresent(
                                detail::pick(slices, id),
                                "a slice of scatter")));
            };

        return ArraySignal<U>::fromElements(detail::makeArrayOnce(
                    array.elements(),
                    [](Element const& element)
                    {
                        return element.id;
                    },
                    std::move(build)));
    }

    /** @brief Fans the elements in, in membership order.
     *
     * The only exit from the array domain, and the reason the element type has
     * to be a signal: an array of anything else has nothing to fan in. Map it
     * to something joinable first.
     *
     * A membership change initializes what arrived and releases what left. A
     * surviving element is carried across untouched, so whatever its signal has
     * accumulated survives its neighbours coming and going without the caller
     * having to share anything.
     */
    template <typename X>
    AnySignal<std::vector<X>> join(ArraySignal<AnySignal<X>> array)
    {
        return wrap(detail::ArrayJoin<X>(array.elements().unwrap()));
    }
} // namespace bq::signal
