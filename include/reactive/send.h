#pragma once

#include "inputresult.h"

#include "signal2/input.h"
#include "stream/handle.h"

#include <ase/pointerbuttonevent.h>
#include <ase/textevent.h>
#include <ase/keyevent.h>

namespace reactive
{
    namespace detail
    {
        template <typename T, typename T2>
        void push(stream::Handle<T> h, T2&& t)
        {
            h.push(std::forward<T2>(t));
        }

        template <typename T, typename T2>
        void push(signal2::InputHandle<T> h, T2&& t)
        {
            h.set(std::forward<T2>(t));
        }

        template <typename THandle, typename TValue>
        class Send
        {
        public:
            Send(THandle&& handle, TValue&& value) :
                handle_(std::forward<THandle>(handle)),
                value_(std::forward<TValue>(value))
            {
            }

            template <typename... Ts>
            void operator()(Ts&&...)
            {
                push(handle_, value_);
            }

        private:
            typename std::decay<THandle>::type handle_;
            typename std::decay<TValue>::type value_;
        };

        template <typename THandle>
        class Forward
        {
        public:
            Forward(THandle&& handle) :
                handle_(std::forward<THandle>(handle))
            {
            }

            template <typename T>
            void operator()(T&& t)
            {
                push(handle_, std::forward<T>(t));
            }

        private:
            typename std::decay<THandle>::type handle_;
        };

    } // detail

    template <typename T>
    auto send(T&& t, stream::Handle<T> const& handle)
        -> detail::Send<stream::Handle<T> const&, T>
    {
        return detail::Send<stream::Handle<T> const&, T>(handle,
                std::forward<T>(t));
    }

    template <typename T, typename U, typename = std::enable_if_t<
        std::is_convertible_v<U, T>
        >>
    auto send(U&& u, signal2::InputHandle<T> const& handle)
        -> detail::Send<
            signal2::InputHandle<std::decay_t<T>> const&, U
        >
    {
        return detail::Send<
            signal2::InputHandle<std::decay_t<T>> const&,
            U>(handle, std::forward<U>(u));
    }

    template <typename T>
    auto send(signal2::InputHandle<T> const& handle)
        -> detail::Forward<signal2::InputHandle<T> const&>
    {
        return detail::Forward<signal2::InputHandle<T> const&>(handle);
    }

    template <typename T>
    auto send(stream::Handle<T> const& handle)
        -> detail::Forward<stream::Handle<T> const&>
    {
        return detail::Forward<stream::Handle<T> const&>(handle);
    }

    namespace detail
    {
        template <typename T>
        struct KeySender
        {
            auto operator()(ase::KeyEvent const& e) const -> InputResult
            {
                handle.push(e);
                return InputResult::handled;
            }

            auto operator()(ase::TextEvent const& e) const -> InputResult
            {
                handle.push(e);
                return InputResult::handled;
            }

            mutable stream::Handle<T> handle;
        };
    } // detail

    template <typename T>
    auto sendKeysTo(stream::Handle<T> handle)
        -> detail::KeySender<T>
    {
        return detail::KeySender<T>{std::move(handle)};
    }
}

