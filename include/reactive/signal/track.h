#pragma once

#include "databind.h"

#include <fit/identity.h>

#include <atomic>

namespace reactive
{
    namespace signal
    {
        template <typename TCollection>
        class Track
        {
        public:
            using reader_t = decltype(btl::reader(std::declval<TCollection>()));
            using value_t = typename std::decay<decltype(
                    std::declval<reader_t>()[0])>::type;
            using collection_connection_t = decltype(btl::observe(
                        std::declval<TCollection const>(),
                        std::function<void(
                            btl::collection_event<value_t> const&)>()));

            Track(TCollection&& collection) :
                collection_(std::forward<TCollection>(collection)),
                collectionConnection_(btl::observe(collection_, std::bind(
                                &Track::onCollectionChanged,
                                this, std::placeholders::_1))),
                collectionChanged_(false)
            {
                auto r = btl::reader(collection_);
                values_ = std::vector<value_t>(r.begin(), r.end());
            }

            Track(Track const& rhs) :
                observable_(rhs.observable_),
                collection_(rhs.collection_),
                values_(rhs.values_),
                collectionConnection_(btl::observe(collection_, std::bind(
                                &Track::onCollectionChanged,
                                this, std::placeholders::_1))),
                collectionChanged_(rhs.collectionChanged_ == true),
                changed_(rhs.changed_)
            {
            }

            Track(Track&& rhs) noexcept :
                observable_(rhs.observable_),
                collection_(rhs.collection_),
                values_(rhs.values_),
                collectionConnection_(btl::observe(collection_, std::bind(
                                &Track::onCollectionChanged,
                                this, std::placeholders::_1))),
                collectionChanged_(rhs.collectionChanged_ == true),
                changed_(rhs.changed_)
            {
            }

            Track& operator=(Track const& rhs)
            {
                observable_ = rhs.observable_;
                collection_ = rhs.collection_;
                values_ = rhs.values_;
                collectionConnection_(btl::observe(collection_, std::bind(
                                &Track::onCollectionChanged,
                                this, std::placeholders::_1)));
                collectionChanged_ = rhs.collectionChanged_;
                changed_ = rhs.changed_;

                return *this;
            }

            Track& operator=(Track&& rhs) noexcept
            {
                observable_ = rhs.observable_;
                collection_ = rhs.collection_;
                values_ = rhs.values_;
                collectionConnection_(btl::observe(collection_, std::bind(
                                &Track::onCollectionChanged,
                                this, std::placeholders::_1)));
                collectionChanged_ = rhs.collectionChanged_;
                changed_ = rhs.changed_;

                return *this;
            }

            std::vector<value_t> const& evaluate() const
            {
                return values_;
            }

            bool hasChanged() const
            {
                return changed_;
            }

            void beginTransaction()
            {
            }

            btl::option<signal_time_t> endTransaction(signal_time_t)
            {
                changed_ = false;

                bool changed = true;
                if (collectionChanged_.compare_exchange_strong(changed, false))
                {
                    changed_ = true;
                    auto r = btl::reader(collection_);
                    values_ = std::vector<value_t>(r.begin(), r.end());
                }

                return btl::none;
            }

            template <typename TCallback>
            Connection observe(TCallback&& cb)
            {
                return observable_->observe(std::forward<TCallback>(cb));
            }

            Annotation annotate() const
            {
                Annotation a;
                a.addNode("track<" + btl::demangle<TCollection>()
                        + "> changed: " + std::to_string(hasChanged()));
                return a;
            }

        private:
            void onCollectionChanged(btl::collection_event<value_t> const&)
            {
                collectionChanged_ = true;
                observable_->notify();
            }

        private:
            btl::shared<Observable> observable_ =
                std::make_shared<Observable>();
            typename std::decay<TCollection>::type collection_;
            std::vector<value_t> values_;
            btl::connection collectionConnection_;
            std::atomic<bool> collectionChanged_;
            bool changed_ = false;
        };

        template <typename TCollection, typename = typename std::enable_if
            <
                 btl::IsCollection<TCollection>::value
            >::type>
        auto track(TCollection&& collection)
        -> Track<TCollection>
        {
            return Track<TCollection>(std::forward<TCollection>(collection));
        }

#if 0
        template <typename TCollection, typename = typename std::enable_if
            <
                 btl::IsCollection<TCollection>::value
            >::type>
        auto track(TCollection&& collection)
            -> decltype(dataBind(collection, fit::identity))
        {
            return dataBind(std::forward<TCollection>(collection),
                    fit::identity);
        }
#endif
    }
}

