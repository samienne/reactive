#pragma once

#include "stream.h"

#include <reactive/signal/signal.h>
#include <reactive/signal/signaltraits.h>

#include <btl/demangle.h>
#include <btl/spinlock.h>
#include <btl/shared.h>

#include <mutex>

namespace reactive
{
    namespace stream
    {
        template <typename T>
        class Collect;
    }

    template <typename T>
    struct signal::IsSignal<stream::Collect<T>> : std::true_type {};
}

namespace reactive::stream
{
    template <typename T>
    class Collect
    {
        struct Control
        {
            std::vector<std::decay_t<T>> values;
            std::vector<std::decay_t<T>> newValues;
            std::vector<std::pair<uint32_t, std::function<void()>>> callbacks;
            btl::SpinLock mutex;
            uint64_t frameId = 0;
            uint32_t nextId = 1;
        };

        static bool pushValue(Control& control, T value)
        {
            decltype(control_->callbacks) callbacks;

            {
                std::unique_lock<btl::SpinLock> lock(control.mutex);
                control.newValues.push_back(std::forward<T>(value));

                callbacks = std::move(control.callbacks);
            }

            for (auto&& cb : callbacks)
                cb.second();

            return true;
        }

    public:
        Collect(Stream<T> stream) :
            control_(std::make_shared<Control>()),
            stream_(std::move(stream)
                    .fmap([control=&*control_](T value)
                    {
                        return pushValue(*control, std::forward<T>(value));
                    }))
        {
        }

        Collect(Collect const&) = default;
        Collect(Collect&&) noexcept = default;
        Collect& operator=(Collect const&) = default;
        Collect& operator=(Collect&&) noexcept = default;

        std::vector<std::decay_t<T>> const& evaluate() const
        {
            return control_->values;
        }

        bool hasChanged() const
        {
            return !control_->values.empty();
        }

        signal::UpdateResult updateBegin(signal::FrameInfo const& frame)
        {
            std::unique_lock<btl::SpinLock> lock(control_->mutex);
            if (control_->frameId == frame.getFrameId())
                return btl::none;

            control_->frameId = frame.getFrameId();
            control_->values.clear();
            control_->values.swap(control_->newValues);
            return btl::none;
        }

        signal::UpdateResult updateEnd(signal::FrameInfo const&)
        {
            return btl::none;
        }

        template <typename TCallback>
        Connection observe(TCallback&& callback)
        {
            uint32_t id = 0;
            std::weak_ptr<Control> control = control_.ptr();

            {
                std::unique_lock<btl::SpinLock>(control_->mutex);
                id = control_->nextId++;
                control_->callbacks.push_back(std::make_pair(id,
                            std::forward<TCallback>(callback)));
            }

            return Connection::on_disconnect([id, control]() mutable
            {
                if (auto p = control.lock())
                {
                    std::unique_lock<btl::SpinLock>(p->mutex);
                    for (auto i = p->callbacks.begin();
                        i != p->callbacks.end(); ++i)
                    {
                        if (id == i->first)
                        {
                            p->callbacks.erase(i);
                            break;
                        }
                    }
                }
            });
        }

        Annotation annotate() const
        {
            Annotation a;
            a.addNode("collect<" + btl::demangle<Stream<T>>() + ">");
            return a;
        }

        Collect clone() const
        {
            return *this;
        }

    private:
        btl::shared<Control> control_;
        Stream<bool> stream_;
    };

    template <typename T>
    auto collect(Stream<T> stream)
    {
        return signal::wrap(Collect<T>(std::move(stream)));
    }
} // reactive::stream

