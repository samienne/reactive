#pragma once

#include "stream.h"

#include <reactive/signal/input.h>
#include <reactive/signal/signaltraits.h>

#include "reactive/connection.h"

namespace reactive
{
    namespace stream
    {
        template <typename T>
        class Hold;
    }

    namespace signal
    {
        template <typename T>
        struct IsSignal<stream::Hold<T>> : std::true_type {};
    }
}

namespace reactive::stream
{
    template <typename T>
    class Hold
    {
        struct Setter
        {
            Setter(signal::InputHandle<T> handle) :
                handle(std::move(handle))
            {
            }

            bool operator()(T value) const
            {
                handle.set(std::forward<T>(value));
                return true;
            }

            mutable signal::InputHandle<T> handle;
        };

    public:
        Hold(Stream<T>&& stream, T&& initial) :
            input_(signal::input(std::move(initial))),
            stream_(std::move(stream)
                    .fmap(Setter(std::move(input_.handle))))
        {
        }

        //Hold(Hold const&) = delete;
        //Hold(Hold&&) = default;
        //Hold& operator=(Hold const&) = delete;
        //Hold& operator=(Hold&&) = default;

        T evaluate() const
        {
            return input_.signal.evaluate();
        }

        bool hasChanged() const
        {
            return input_.signal.hasChanged();
        }

        signal::UpdateResult updateBegin(signal::FrameInfo const& frame)
        {
            return input_.signal.updateBegin(frame);
        }

        signal::UpdateResult updateEnd(signal::FrameInfo const& frame)
        {
            return input_.signal.updateEnd(frame);
        }

        template <typename TCallback>
        Connection observe(TCallback callback)
        {
            return input_.signal.observe(std::forward<TCallback>(callback));
        }

        Annotation annotate() const
        {
            Annotation a;
            a.addNode(this, "hold()");
            return a;
        }

    private:
        signal::Input<T> input_;
        Stream<bool> stream_;
    };

    template <typename T>//, typename TInitial>
    auto hold(Stream<T>&& stream, T&& initial)
    {
        return Hold<T>(std::move(stream), std::forward<T>(initial));
    }
} // namespace reactive::stream


