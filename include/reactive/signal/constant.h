#pragma once

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
                constant_(std::forward<T>(initial))
            {
            }

            Constant(Constant<T>&&) = default;
            Constant<T>& operator=(Constant<T>&&) = default;

            btl::decay_t<T> const& evaluate() const
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
            btl::decay_t<T> constant_;
        };

        static_assert(IsSignal<Constant<int>>::value,
                "Constant is not a signal");

        template <typename T>
        Constant<T> constant(T&& value)
        {
            return Constant<T>(std::forward<T>(value));
        }

        template <typename T>
        Constant<std::vector<typename std::decay<T>::type>> constant(
                std::initializer_list<T> v)
        {
            return constant(std::vector<typename std::decay<T>::type>(v));
        }
    }
}

