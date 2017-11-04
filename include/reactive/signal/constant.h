#pragma once

#include <reactive/signal2.h>

#include <reactive/signaltraits.h>
#include <reactive/connection.h>

#include <btl/cloneoncopy.h>
#include <btl/demangle.h>

#include <memory>

namespace reactive
{
    namespace signal
    {
        template <typename T>
        class Constant
        {
        public:
            Constant(T&& initial) :
                constant_(std::move(initial))
            {
            }

            Constant(T const& initial) :
                constant_(initial)
            {
            }

            Constant(Constant<T>&& rhs) noexcept(true) :
                constant_(std::move(rhs.constant_))
            {
            }

            Constant<T>& operator=(Constant<T>&& rhs) noexcept(true)
            {
                constant_ = std::move(rhs.constant_);
                return *this;
            }

            T const& evaluate() const
            {
                return constant_;
            }

            bool hasChanged() const
            {
                return false;
            }

            UpdateResult updateBegin(signal::FrameInfo const&)
            {
                return btl::none; }

            UpdateResult updateEnd(signal::FrameInfo const&)
            {
                return btl::none;
            }

            template <typename TCallback>
            Connection observe(TCallback&&)
            {
                // nothing to observe
                return Connection();
            }

            Annotation annotate() const
            {
                Annotation a;
                a.addNode("constant<" + btl::demangle<T>() + ">");
                return a;
            }

            Constant clone() const
            {
                return *this;
            }

        private:
            Constant(Constant<T> const&) = default;
            Constant<T>& operator=(Constant<T> const&) = default;

        private:
            T constant_;
        };

        static_assert(IsSignal<Constant<int>>::value,
                "Constant is not a signal");

        template <typename T>
        auto constant(T&& value)
        {
            return signal2::wrap(Constant<std::decay_t<T>>(
                        std::forward<T>(value)
                        ));
        }

        template <typename T>
        /*Constant<std::vector<typename std::decay<T>::type>>*/ auto constant(
                std::initializer_list<T> v)
        {
            return constant(std::vector<std::decay_t<T>>(v));
        }
    }
}

