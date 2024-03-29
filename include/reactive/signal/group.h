#include "btl/cloneoncopy.h"
#include <type_traits>
#pragma once

#include "reactive/signal/signal.h"

#include <btl/tuplereduce.h>
#include <btl/tuplemap.h>

#include <tuple>

namespace reactive::signal
{
    template <typename... Ts>
    class Group
    {
        static_assert(std::is_nothrow_move_assignable_v<std::tuple<Ts...>>);

    public:
        Group(Ts... sigs) : sigs_(std::make_tuple(std::move(sigs)...))
        {
        }

        Group(Group const&) = default;
        Group(Group&&) noexcept = default;

        Group& operator=(Group const&) = default;
        Group& operator=(Group&&) noexcept = default;

        auto evaluate() const
        {
            return makeSignalResultFromTuple(btl::tuple_map(*sigs_,
                    [](auto const& sig) -> decltype(auto)
                    {
                        return sig.evaluate();
                    }));
        }

        bool hasChanged() const
        {
            return btl::tuple_reduce(false, *sigs_,
                    [](bool r, auto&& s)
                    {
                        return r || s.hasChanged();
                    });
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            return btl::tuple_reduce(UpdateResult(std::nullopt), *sigs_,
                    [&frame](UpdateResult r, auto&& s)
                    {
                        return min(r, s.updateBegin(frame));
                    });
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            return btl::tuple_reduce(UpdateResult(std::nullopt), *sigs_,
                    [&frame](UpdateResult r, auto&& s)
                    {
                        return min(r, s.updateEnd(frame));
                    });
        }

        template <typename TCallback>
        btl::connection observe(TCallback&& callback)
        {
            return btl::tuple_reduce(Connection(), *sigs_,
                    [callback=std::forward<TCallback>(callback)](Connection r, auto&& s)
                    {
                        return std::move(r) + s.observe(callback);
                    });
        }

        Annotation annotate() const
        {
            Annotation a;
            //auto&& n = a.addNode("Signal<" + btl::demangle<T>()
                    //+ "> changed: " + std::to_string(hasChanged()));
            //a.addShared(sig_.raw_ptr(), n, deferred_->annotate());
            return a;
        }

        Group clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<std::tuple<Ts...>> sigs_;
    };

    template <typename... Ts, typename... Us>
    auto group(Signal<Ts, Us>... sigs)
    {
        return signal::wrap(Group<Signal<Ts, Us>...>(std::move(sigs)...));
    }

    template <typename T, typename U>
    auto group(Signal<T, U> sig)
    {
        return sig;
    }
} // reactive::signal

