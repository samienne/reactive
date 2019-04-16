#pragma once

#include "reactive/signal.h"
#include "reactive/signaltraits.h"

#include <btl/hidden.h>

namespace reactive
{
    namespace signal
    {
        template <typename T>
        class Weak;
    }

    template <typename T>
    struct IsSignal<signal::Weak<T>> : std::true_type {};
}

namespace reactive::signal
{
    template <typename T>
    class Weak
    {
    public:
        Weak()
        {
        }

        Weak(std::weak_ptr<SignalBase<T>> sig) :
            deferred_(std::move(sig))
        {
        }

        template <typename U>
        Weak(SharedSignal<T, U> const& sig) :
            deferred_(sig.signal().weak())
        {
        }

        auto evaluate() const
            -> decltype(std::declval<SignalBase<T>>().evaluate())
        {
            auto p = deferred_.lock();
            assert(p);

            return p->evaluate();
        }

        bool hasChanged() const
        {
            if (auto p = deferred_.lock())
                return p->hasChanged();
            else
                return false;
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            if (auto p = deferred_.lock())
                return p->updateBegin(frame);
            else
                return btl::none;
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            if (auto p = deferred_.lock())
                return p->updateEnd(frame);
            else
                return btl::none;
        }

        template <typename TCallback>
        Connection observe(TCallback&& callback)
        {
            if (auto p = deferred_.lock())
                return p->observe(std::forward<TCallback>(callback));
            else
                return Connection();
        }

        Annotation annotate() const
        {
            if (auto p = deferred_.lock())
            {
                Annotation a;
                auto&& n = a.addNode("weak()");
                a.addShared(p.get(), n, p->annotate());
                return a;
            }

            Annotation a;
            a.addNode("weak(disconnected)");
            return a;
        }

        Weak clone() const
        {
            return *this;
        }

        bool operator==(Weak const& rhs) const
        {
            return deferred_.lock().get() == rhs.deferred_.lock().get();
        }

        bool operator!=(Weak const& rhs) const
        {
            return !(*this == rhs);
        }

        bool isValid() const
        {
            return !deferred_.expired();
        }

    private:
        std::weak_ptr<SignalBase<T>> deferred_;
    };

    template <typename T, typename U>
    auto weak(SharedSignal<T, U> const& sig) // -> Weak<T>
    {
        return signal::wrap(Weak<T>(sig));
    }
} // namespace reactive::signal

