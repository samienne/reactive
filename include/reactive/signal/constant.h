#pragma once

#include <reactive/signal.h>

#include <reactive/signaltraits.h>
#include <reactive/connection.h>

#include <btl/cloneoncopy.h>
#include <btl/demangle.h>
#include <btl/hidden.h>

#include <memory>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive::signal
{
    template <typename T>
    class BTL_CLASS_VISIBLE Constant
    {
    public:
        BTL_HIDDEN Constant(T&& initial) :
            constant_(std::move(initial))
        {
        }

        BTL_HIDDEN Constant(T const& initial) :
            constant_(initial)
        {
        }

        BTL_HIDDEN Constant(Constant<T>&& rhs) noexcept(true) :
            constant_(std::move(rhs.constant_))
        {
        }

        BTL_HIDDEN Constant<T>& operator=(Constant<T>&& rhs) noexcept(true)
        {
            constant_ = std::move(rhs.constant_);
            return *this;
        }

        BTL_HIDDEN T const& evaluate() const
        {
            return constant_;
        }

        BTL_HIDDEN bool hasChanged() const
        {
            return false;
        }

        BTL_HIDDEN UpdateResult updateBegin(signal::FrameInfo const&)
        {
            return btl::none; }

        BTL_HIDDEN UpdateResult updateEnd(signal::FrameInfo const&)
        {
            return btl::none;
        }

        template <typename TCallback>
        BTL_HIDDEN Connection observe(TCallback&&)
        {
            // nothing to observe
            return Connection();
        }

        BTL_HIDDEN Annotation annotate() const
        {
            Annotation a;
            a.addNode("constant<" + btl::demangle<T>() + ">");
            return a;
        }

        BTL_HIDDEN Constant clone() const
        {
            return *this;
        }

    private:
        BTL_HIDDEN Constant(Constant<T> const&) = default;
        BTL_HIDDEN Constant<T>& operator=(Constant<T> const&) = default;

    private:
        T constant_;
    };

    template <typename T>
    auto constant(T&& value)
    {
        return signal::wrap(Constant<std::decay_t<T>>(
                    std::forward<T>(value)
                    ));
    }

    template <typename T>
    /*Constant<std::vector<typename std::decay<T>::type>>*/ auto constant(
            std::initializer_list<T> v)
    {
        return constant(std::vector<std::decay_t<T>>(v));
    }
} // reactive::signal

namespace reactive
{
    template <typename T>
    struct IsSignal<signal::Constant<T>> : std::true_type {};

    /*
    static_assert(IsSignal<Constant<int>>::value,
            "Constant is not a signal");
            */
}

BTL_VISIBILITY_POP

