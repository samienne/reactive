#pragma once

#include "arraysignal.h"
#include "combine.h"
#include "constant.h"
#include "join.h"
#include "merge.h"
#include "signal.h"

#include <cassert>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

/** @brief The fan-in / fan-out pair a container implementation needs.
 *
 * Everything here is for code that computes over a whole collection at once —
 * a layout deciding where its children go — and hands each element its own
 * share of the result. An ordinary caller reaching for `scatter` almost always
 * wants `ArraySignal::map` instead.
 *
 * The separation is by header and namespace rather than by the include
 * firewall: ArraySignal lives in `bq` and layouts live above it, so anything
 * kept out of `bq`'s include directory would be unreachable from exactly the
 * code that needs it. Writing the extra include is the declaration of intent.
 */
namespace bq::signal::layout
{
    /** @brief An element's identity as `mapKeyed` sees it.
     *
     * Opaque: comparable and orderable so it can be matched and stored, and
     * meaningless otherwise. Nothing may be inferred from its value, from its
     * ordering relative to another key, or from its stability across runs.
     */
    class ArrayKey
    {
    public:
        explicit ArrayKey(ArrayId id) :
            id_(std::move(id))
        {
        }

        bool operator==(ArrayKey const& rhs) const
        {
            return id_ == rhs.id_;
        }

        bool operator!=(ArrayKey const& rhs) const
        {
            return id_ != rhs.id_;
        }

        bool operator<(ArrayKey const& rhs) const
        {
            return id_ < rhs.id_;
        }

        /** @brief The identity behind the key. Not for consumers of the key. */
        ArrayId const& id() const
        {
            return id_;
        }

    private:
        ArrayId id_;
    };

    /** @brief One value per element of an ArraySignal, with identity attached.
     *
     * Produced by `extract` and consumed by `scatter`. It is a sibling of
     * ArraySignal, not a subtype: an ArraySignal is operated on per element,
     * whereas a KeyedArraySignal is operated on as a whole vector, because that
     * is what a layout computation needs. There is no `forEach` here, and that
     * is deliberate.
     *
     * The alignment between the values and the elements they came from is
     * carried by the type rather than by an assertion at the point of use:
     * only something `extract` produced can be scattered back.
     */
    template <typename A>
    class KeyedArraySignal
    {
    public:
        using AggregateSignal = AnySignal<std::vector<ArrayId>,
              std::vector<A>>;

        /** @brief Wraps an aggregate an operator produced.
         *
         * Not part of the caller-facing surface.
         */
        static KeyedArraySignal fromSignal(AggregateSignal sig)
        {
            return KeyedArraySignal(std::move(sig));
        }

        /** @brief Computes over every element's value at once.
         *
         * `g` is called as `g(extras..., values)` — the ambient signals first,
         * the per-element vector last — and must return one value per input
         * value, positionally aligned with it. The input is the current
         * membership, not the membership at the time the node was built. The
         * size is asserted; the alignment cannot be checked and is the
         * function's contract to keep. A computation that reorders or drops
         * elements wants `mapKeyed` instead.
         *
         * `extras` are ambient signals injected alongside the vector — for a
         * layout, the container's own size.
         */
        template <typename TFunc, typename... TExtras>
        auto mapValues(TFunc g, TExtras... extras) const
        {
            using Result = std::decay_t<std::invoke_result_t<TFunc,
                  SingleSignalTypeT<std::decay_t<TExtras>> const&...,
                  std::vector<A> const&>>;
            using B = typename Result::value_type;

            auto sig = merge(sig_, std::move(extras)...)
                .map([g](std::vector<ArrayId> const& ids,
                            std::vector<A> const& values,
                            auto const&... extraValues)
                    {
                        std::vector<B> result = g(extraValues..., values);
                        assert(result.size() == values.size()
                                && "mapValues: size must be preserved");

                        return makeSignalResult(ids, std::move(result));
                    });

            return KeyedArraySignal<B>::fromSignal(std::move(sig));
        }

        /** @brief Computes over every element's value, carrying the key.
         *
         * `h` maps `vector<pair<ArrayKey, A>>` to `vector<pair<ArrayKey, B>>`
         * and may reorder or drop entries. Identity is preserved for the keys
         * `h` returns; the rest are dropped, and an element whose key is
         * dropped disappears from a later `scatter`.
         *
         * Use this rather than `mapValues` whenever the computation needs to
         * match values across a membership change — position does not survive
         * an insertion or a reorder, so a fold that pairs the current vector
         * with the previous one by position pairs different elements.
         */
        template <typename TFunc>
        auto mapKeyed(TFunc h) const
        {
            using Result = std::decay_t<std::invoke_result_t<TFunc,
                  std::vector<std::pair<ArrayKey, A>> const&>>;
            using B = typename Result::value_type::second_type;

            auto sig = sig_.map([h](std::vector<ArrayId> const& ids,
                        std::vector<A> const& values)
                {
                    std::vector<std::pair<ArrayKey, A>> input;
                    input.reserve(ids.size());
                    for (size_t i = 0; i < ids.size(); ++i)
                        input.emplace_back(ArrayKey(ids[i]), values[i]);

                    auto output = h(input);

                    std::vector<ArrayId> outIds;
                    std::vector<B> outValues;
                    outIds.reserve(output.size());
                    outValues.reserve(output.size());
                    for (auto& entry : output)
                    {
                        outIds.push_back(entry.first.id());
                        outValues.push_back(std::move(entry.second));
                    }

                    return makeSignalResult(std::move(outIds),
                            std::move(outValues));
                });

            return KeyedArraySignal<B>::fromSignal(std::move(sig));
        }

        /** @brief Leaves the domain, discarding identity.
         *
         * The one-way escape back to a plain signal. Entering the domain needs
         * a key because only the caller knows what makes two items the same
         * thing; leaving it needs nothing, because forgetting is free.
         */
        AnySignal<std::vector<A>> values() const
        {
            return sig_.map([](std::vector<ArrayId> const&,
                        std::vector<A> const& values)
                {
                    return values;
                });
        }

        /** @brief The identities and values together.
         *
         * An operator's entry point into the aggregate, not part of the
         * caller-facing surface.
         */
        AggregateSignal const& sig() const
        {
            return sig_;
        }

    private:
        explicit KeyedArraySignal(AggregateSignal sig) :
            sig_(std::move(sig).share())
        {
        }

        AggregateSignal sig_;
    };

    /** @brief Order-preserving fan-in: one signal per element, gathered.
     *
     * `f` is applied **once per identity**, so the per-element signal it
     * returns is initialised once and thereafter merely updates. That is not an
     * optimisation: it is what stops a sibling insertion from resetting every
     * surviving element's state.
     *
     * `f` takes the element's value signal and returns the signal to gather.
     * A plain `A(T)` is accepted too and is mapped over the element's value.
     */
    template <typename T, typename TFunc>
    auto extract(ArraySignal<T> const& array, TFunc f)
    {
        constexpr bool takesSignal = std::is_invocable_v<TFunc, AnySignal<T>>;

        auto perIdentity = [f](ArrayId const&, AnySignal<T> const& value)
        {
            if constexpr (takesSignal)
                return AnySignal<SingleSignalTypeT<std::decay_t<
                    std::invoke_result_t<TFunc, AnySignal<T>>>>>(
                            f(value).share());
            else
                return AnySignal<std::decay_t<std::invoke_result_t<TFunc,
                       T const&>>>(value.map(f).share());
        };

        using A = SingleSignalTypeT<std::decay_t<
            std::invoke_result_t<decltype(perIdentity), ArrayId const&,
            AnySignal<T> const&>>>;

        auto sig = detail::mapElementsOnce<A>(array.elements(),
                perIdentity)
            .map([](detail::ArrayElements<A> const& elements)
                {
                    std::vector<ArrayId> ids;
                    std::vector<AnySignal<A>> sigs;
                    ids.reserve(elements.size());
                    sigs.reserve(elements.size());
                    for (auto const& element : elements)
                    {
                        ids.push_back(element.id);
                        sigs.push_back(element.value);
                    }

                    return makeSignalResult(std::move(ids),
                            AnySignal<std::vector<A>>(combine(
                                    std::move(sigs))));
                })
            .join();

        return KeyedArraySignal<A>::fromSignal(std::move(sig));
    }

    /** @brief Identity-matched fan-out: `map` with one extra argument.
     *
     * `f` receives the element's own value signal and the element's own share
     * of the aggregate, and is applied once per identity — as is the matching
     * itself, which happens when the identity appears rather than on every
     * emission. An element whose identity is not in the aggregate (because
     * `mapKeyed` dropped it) is absent from the result.
     *
     * The name is a mild deterrent by design: a caller who reaches for it
     * usually wants `ArraySignal::map` and does not yet know it.
     */
    template <typename T, typename B, typename TFunc>
    auto scatter(ArraySignal<T> const& array, KeyedArraySignal<B> const& keyed,
            TFunc f)
    {
        using U = std::decay_t<std::invoke_result_t<TFunc, AnySignal<T>,
              AnySignal<B>>>;

        auto byId = AnySignal<std::map<ArrayId, B>>(keyed.sig()
                .map([](std::vector<ArrayId> const& ids,
                            std::vector<B> const& values)
                    {
                        std::map<ArrayId, B> result;
                        for (size_t i = 0; i < ids.size(); ++i)
                            result.emplace(ids[i], values[i]);

                        return result;
                    })
                .share());

        auto once = std::make_shared<detail::OncePerId<T, U,
             std::function<AnySignal<U>(ArrayId const&,
                     AnySignal<T> const&)>>>(
                [f, byId](ArrayId const& id, AnySignal<T> const& value)
                {
                    auto slice = AnySignal<B>(byId
                            .map([id](std::map<ArrayId, B> const& all) -> B
                                {
                                    auto i = all.find(id);
                                    assert(i != all.end()
                                            && "scatter: unmatched identity");
                                    return i->second;
                                })
                            .share());

                    return AnySignal<U>(constant(f(value, std::move(slice))));
                });

        auto elements = merge(array.elements(), keyed.sig())
            .map([once](detail::ArrayElements<T> const& all,
                        std::vector<ArrayId> const& ids,
                        std::vector<B> const&)
                {
                    std::set<ArrayId> present(ids.begin(), ids.end());

                    detail::ArrayElements<T> live;
                    live.reserve(all.size());
                    for (auto const& element : all)
                    {
                        if (present.count(element.id))
                            live.push_back(element);
                    }

                    return once->apply(live);
                });

        return ArraySignal<U>::fromElements(std::move(elements));
    }
} // namespace bq::signal::layout
