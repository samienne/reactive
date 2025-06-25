#pragma once

#include "inputresult.h"

#include <bq/signal/input.h>
#include <bq/stream/handle.h>

#include <ase/pointerbuttonevent.h>
#include <ase/textevent.h>
#include <ase/keyevent.h>

namespace bqui
{
    namespace detail
    {
        template <typename T, typename T2>
        void push(bq::stream::Handle<T> h, T2&& t)
        {
            h.push(std::forward<T2>(t));
        }

        template <typename T, typename T2>
        void push(bq::signal::InputHandle<T> h, T2&& t)
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
    auto send(T&& t, bq::stream::Handle<T> const& handle)
        -> detail::Send<bq::stream::Handle<T> const&, T>
    {
        return detail::Send<bq::stream::Handle<T> const&, T>(handle,
                std::forward<T>(t));
    }

    template <typename T, typename U, typename = std::enable_if_t<
        std::is_convertible_v<U, T>
        >>
    auto send(U&& u, bq::signal::InputHandle<T> const& handle)
        -> detail::Send<
            bq::signal::InputHandle<std::decay_t<T>> const&, U
        >
    {
        return detail::Send<
            bq::signal::InputHandle<std::decay_t<T>> const&,
            U>(handle, std::forward<U>(u));
    }

    template <typename T>
    auto send(bq::signal::InputHandle<T> const& handle)
        -> detail::Forward<bq::signal::InputHandle<T> const&>
    {
        return detail::Forward<bq::signal::InputHandle<T> const&>(handle);
    }

    template <typename T>
    auto send(bq::stream::Handle<T> const& handle)
        -> detail::Forward<bq::stream::Handle<T> const&>
    {
        return detail::Forward<bq::stream::Handle<T> const&>(handle);
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

            mutable bq::stream::Handle<T> handle;
        };
    } // detail

    template <typename T>
    auto sendKeysTo(bq::stream::Handle<T> handle)
        -> detail::KeySender<T>
    {
        return detail::KeySender<T>{std::move(handle)};
    }
}

