#pragma once

#include "conditional.h"
#include "cache.h"
#include "droprepeats.h"
#include "map.h"
#include "input.h"
#include "inputhandle.h"

#include "reactive/signaltype.h"
#include "reactive/signaltraits.h"

#include <btl/typetraits.h>
#include <btl/observable.h>
#include <btl/collection.h>
#include <btl/option.h>

#include <fit/compose.h>

#include <type_traits>
#include <utility>

namespace reactive
{
    namespace signal
    {
        using IndexSignal = signal2::Signal<size_t>;

        template <typename TDelegate, typename TCollection>
        class DataBind;

        template <typename TDelegate, typename TCollection/*,
                 typename = typename std::enable_if
                     <
                        btl::IsCollection<TCollection>::value
                     >::type*/>
        class DataBindPrivate
        {
        public:
            using CollectionType = std::decay_t<TCollection>;
            using ItemType = std::decay_t<
                btl::CollectionValueType<CollectionType>
                >;

            using DelegateType = std::decay_t<TDelegate>;
            using DelegateReturnType = std::decay_t<decltype(
                    std::declval<DelegateType>()(
                        std::declval<signal2::SharedSignal<btl::option<ItemType>>>(),
                        std::declval<IndexSignal>()
                        )
                )>;

            using DelegateOptionType = std::decay_t<decltype(
                    std::declval<DelegateReturnType>().evaluate()
                )>;

            using DelegateValueType = std::decay_t<decltype(
                    *std::declval<DelegateOptionType>()
                )>;

        private:
            struct Sig
            {
                std::decay_t<decltype(
                        share(std::declval<DelegateReturnType>())
                        )> sig;
                InputHandle<size_t> indexHandle;
                bool alive;
            };

        public:
            DataBindPrivate(TDelegate delegate, TCollection collection):
                delegate_(std::move(delegate)),
                collection_(std::move(collection)),
                collectionChanged_(false)
            {
                size_t n = 0;
                for (auto const& item : btl::reader(collection_))
                {
                    auto i = input(btl::just(item));
                    auto index = input<size_t>(n++);
                    auto indexSignal = signal::dropRepeats(
                            std::move(index.signal)
                            );

                    handles_.push_back(i.handle);

                    signals_.push_back({
                            share(delegate_(
                                    std::move(i.signal),
                                    std::move(indexSignal)
                                    )
                                ),
                            index.handle,
                            true
                            });
                }

                connection_ = btl::observe(collection_, std::bind(
                            &DataBindPrivate::onCollectionChanged,
                            this, std::placeholders::_1));
            }

            std::vector<DelegateValueType> evaluate()
            {
                std::vector<DelegateValueType> result;

                size_t i = 0;
                for (auto&& sig : signals_)
                {
                    auto value = btl::clone(sig.sig.evaluate());
                    if (value.valid())
                        result.push_back(std::move(*value));
                    ++i;
                }

                return std::move(result);
            }

            bool hasChanged() const
            {
                return changed_;
            }

            UpdateResult updateBegin(FrameInfo const& frame)
            {
                if (frameId_ == frame.getFrameId())
                    return btl::none;

                frameId_ = frame.getFrameId();

                btl::option<signal_time_t> r(btl::none);
                changed_ = false;

                bool expected = true;
                if (collectionChanged_.compare_exchange_strong(expected, false))
                    changed_ = true;

                if (!events_.empty())
                {
                    for (auto&& event : events_)
                        handleCollectionEvent(event);

                    events_.clear();
                }

                for (auto&& sig : signals_)
                {
                    auto r2 = sig.sig.updateBegin(frame);
                    r = min(r, r2);
                }


                return r;
            }

            UpdateResult updateEnd(FrameInfo const& frame)
            {
                UpdateResult r = btl::none;

                for (auto&& sig : signals_)
                {
                    auto r2 = sig.sig.updateEnd(frame);
                    changed_ = changed_ || sig.sig.hasChanged();

                    r = min(r, r2);
                }

                auto size = signals_.size();
                signals_.erase(std::remove_if(signals_.begin(), signals_.end(),
                            [](Sig const& s) -> bool
                            {
                                return !s.alive && !s.sig.evaluate().valid();
                            }), signals_.end());

                if (size != signals_.size())
                    updateHandles();

                return r;
            }

            template <typename TCallback>
            Connection observe(TCallback&& cb)
            {
                return observable_.observe(std::forward<TCallback>(cb));
            }

            Annotation annotate() const
            {
                Annotation a;
                a.addNode("dataBind vector<"
                        + btl::demangle<DelegateValueType>() + ">");

                return a;
            }

        private:
            void updateHandles()
            {
                for (size_t i = 0; i < signals_.size(); ++i)
                    signals_[i].indexHandle.set(i);
            }

            template <typename T>
            static void eraseIndices(std::vector<T>& vec,
                    std::vector<size_t> const& indices)
            {
                if (indices.empty())
                    return;
                auto head = vec.begin();
                auto tail = head;
                auto index = indices.begin();
                size_t i = 0;
                while (head != vec.end())
                {
                    if (index == indices.end() || i != *index)
                    {
                        if (tail != head)
                            *tail = std::move(*head);
                        ++tail;
                    }
                    else if (index != indices.end() && i == *index)
                        head->set(btl::none);

                    while (index != indices.end() && i >= *index)
                        ++index;

                    ++i;
                    ++head;
                }

                vec.erase(tail, vec.end());
            }

            typename std::vector<Sig>::iterator getSignalsIterator(size_t index)
            {
                auto i = signals_.begin();
                size_t current = 0;
                while (i != signals_.end())
                {
                    if (i->alive)
                    {
                        if (current == index)
                            break;

                        ++current;
                    }

                    ++i;
                }

                return i;
            }

            void onCollectionChanged(
                    btl::collection_event<ItemType> const& e)
            {
                collectionChanged_ = true;
                events_.push_back(btl::persist(e));

                observable_();
            }

            void handleCollectionEvent(
                    btl::persistent_collection_event<ItemType> const& e)
            {
                eraseIndices(handles_, e.removed);

                for (auto&& v : e.removed)
                {
                    auto i = getSignalsIterator(v);
                    if(i == signals_.end())
                    {
                        std::cout << "finding signal: " << v << std::endl;
                        std::cout << "signal count: " << signals_.size() << std::endl;
                        for (auto const& s : signals_)
                        {
                            std::cout << "alive: " << s.alive << std::endl;
                        }
                    }

                    assert(i != signals_.end());
                    i->alive = false;
                }

                for (auto&& v : e.added)
                {
                    auto value = input(btl::just(v.second));
                    auto index = input<size_t>(v.first);
                    handles_.insert(handles_.begin() + v.first, value.handle);

                    auto j = signals_.begin();
                    size_t n = 0;
                    while (j != signals_.end())
                    {
                        if (n == v.first)
                            break;
                        if (j->alive)
                            ++n;
                        ++j;
                    }

                    auto indexSignal = signal::dropRepeats(
                            std::move(index.signal)
                            );

                    signals_.insert(j, {
                            share(delegate_(
                                    std::move(value.signal),
                                    std::move(indexSignal)
                                    )
                                ),
                            index.handle,
                            true});
                }

                if (!e.added.empty())
                    updateHandles();

                for (auto&& v : e.updated)
                    handles_[v.first].set(btl::just(v.second));
            }

        private:
            friend class DataBind<TDelegate, TCollection>;

            uint64_t frameId_ = 0;
            DelegateType delegate_;
            CollectionType collection_;
            btl::observable<void()> observable_;
            std::vector<Sig> signals_;
            std::vector<InputHandle<btl::option<ItemType>>> handles_;
            std::vector<btl::persistent_collection_event<ItemType>> events_;
            btl::connection connection_;

            std::atomic<bool> collectionChanged_;
            bool changed_ = false;
        };

        template <typename TDelegate, typename TCollection>
        class DataBind
        {
        public:
            using PrivateType = DataBindPrivate<TDelegate, TCollection>;
            using ItemType = typename PrivateType::ItemType;
            using CollectionType = typename PrivateType::CollectionType;
            using DelegateType = typename PrivateType::DelegateType;

            using DelegateReturnType = typename PrivateType::DelegateReturnType;
            using DelegateOptionType = typename PrivateType::DelegateOptionType;
            using DelegateValueType = typename PrivateType::DelegateValueType;

            DataBind(TDelegate delegate, TCollection collection) :
                deferred_(std::make_shared<PrivateType>(
                            std::move(delegate),
                            std::move(collection)
                            )
                        )
            {
            }

        public:
            DataBind(DataBind const&) = default;
            DataBind& operator=(DataBind const&) = default;

        public:
            DataBind(DataBind&&) = default;
            DataBind& operator=(DataBind&&) = default;

            auto evaluate() const
                -> decltype(btl::clone(std::declval<PrivateType>().evaluate()))
            {
                return btl::clone(const_cast<PrivateType*>(
                        deferred_.ptr().get())->evaluate());
            }

            bool hasChanged() const
            {
                return deferred_->hasChanged();
            }

            UpdateResult updateBegin(FrameInfo const& frame)
            {
                return deferred_->updateBegin(frame);
            }

            UpdateResult updateEnd(FrameInfo const& frame)
            {
                return deferred_->updateEnd(frame);
            }

            template <typename TCallback>
            Connection observe(TCallback&& cb)
            {
                return deferred_->observe(std::forward<TCallback>(cb));
            }

            Annotation annotate() const
            {
                Annotation a;
                auto&& n = a.addNode("dataBind");
                a.addShared(deferred_.raw_ptr(), n, deferred_->annotate());

                return a;
            }

            template <typename TFirst, typename TSecond>
            class Compose
            {
            public:
                Compose(TFirst&& first,
                        typename std::decay<TSecond>::type const& second) :
                    first_(std::forward<TFirst>(first)),
                    second_(second)

                {
                }

                template <typename TValue>
                auto operator()(TValue&& value, signal::IndexSignal index)
                    -> decltype(std::declval<TFirst>()(std::declval<TSecond>()(
                                    value, index), index))
                {
                    return first_(second_(std::forward<TValue>(value), index),
                            index);
                }

            private:
                std::decay_t<TFirst> first_;
                std::decay_t<TSecond> second_;
            };

            template <typename TAddDelegate,
                     typename TNewDelegate = Compose<TAddDelegate,
                     typename std::decay<TDelegate>::type>
                >
            auto dataBind(TAddDelegate&& delegate) const
                -> DataBind<TNewDelegate,
                    typename std::decay<TCollection>::type const&>
            {
                auto d = TNewDelegate(
                            std::forward<TAddDelegate>(delegate),
                            deferred_->delegate_);
                return DataBind<TNewDelegate,
                       typename std::decay<TCollection>::type const&>(
                        std::move(d), typename std::decay<TCollection>::type(
                            deferred_->collection_));
            }

            DataBind clone() const
            {
                return *this;
            }

        private:
            btl::shared<PrivateType> deferred_;
        };

        static_assert(IsSignal<
                DataBind<
                std::function<signal2::SharedSignal<btl::option<int>>(
                    signal2::Signal<btl::option<std::string>>,
                    IndexSignal)>,
                btl::collection<std::string>
                >>::value,
                "DataBind is not a signal");

        struct IdentityDelegate
        {
            template <typename T>
            T operator()(T&& t, IndexSignal) const
            {
                return std::forward<T>(t);
            }
        };

        /*
         * delegate: std::function<signal<option<R>>(signal<option<T>>)>
         *
         * Delegate parameter will be set to none if the item is removed from
         * the collection. The signal will be kept in the result vector until
         * it returns btl::none.
         *
         * return: signal<vector<R>>
         */
        template <typename TDelegate, typename TCollection, typename =
            std::enable_if_t
            <
                btl::IsCollection<TCollection>::value
            >
        >
        auto dataBind(TCollection collection,
                TDelegate delegate = IdentityDelegate())
            //-> DataBind<TDelegate, TCollection>
        {
            return DataBind<std::decay_t<TDelegate>, std::decay_t<TCollection>>(
                    std::move(delegate),
                    std::move(collection)
                    );
        }

        template <typename TDelegate, typename TDataBind>
        auto dataBind(TDataBind const& bound, TDelegate&& delegate)
            -> decltype(bound.dataBind(std::forward<TDelegate>(delegate)))
        {
            return bound.dataBind(std::forward<TDelegate>(delegate));
        }

        template <typename T, typename = void>
        struct IsDataBindable : std::false_type {};

        template <typename T>
        struct IsDataBindable<T, btl::void_t
        <
            decltype(signal::dataBind(std::declval<T>(), IdentityDelegate()))
        >>: std::true_type {};

        template <typename T, typename = typename std::enable_if
            <
                IsDataBindable<T>::value
            >::type>
        struct DataBindType
        {
            using type = typename std::decay<decltype(
                    dataBind(std::declval<T>(), IdentityDelegate())
                    .evaluate()[0])>::type;
        };
    } // signal
} // reactive

