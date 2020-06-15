#pragma once

#include "signal.h"

#include "signaltraits.h"
#include "reactive/connection.h"

#include <btl/demangle.h>

#include <memory>

namespace reactive::signal
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

        UpdateResult updateBegin(FrameInfo const&)
        {
            return btl::none; }

        UpdateResult updateEnd(FrameInfo const&)
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

    template <typename T>
    auto constant(T&& value)
    {
        return wrap(Constant<std::decay_t<T>>(
                    std::forward<T>(value)
                    ));
    }

    template <typename T>
    auto constant(std::initializer_list<T> v)
    {
        return constant(std::vector<std::decay_t<T>>(v));
    }

    template <typename T>
    struct IsSignal<Constant<T>> : std::true_type {};
} // reactive::signal

