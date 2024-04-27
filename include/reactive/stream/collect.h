#pragma once

#include "stream.h"

#include <reactive/signal2/signalresult.h>
#include <reactive/signal2/updateresult.h>
#include <reactive/signal2/frameinfo.h>
#include <reactive/signal2/signal.h>

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
                return std::nullopt;

            control_->frameId = frame.getFrameId();
            control_->values.clear();
            control_->values.swap(control_->newValues);
            return std::nullopt;
        }

        signal::UpdateResult updateEnd(signal::FrameInfo const&)
        {
            return std::nullopt;
        }

        template <typename TCallback>
        Connection observe(TCallback&& callback)
        {
            uint32_t id = 0;
            std::weak_ptr<Control> control = control_.ptr();

            {
                std::unique_lock<btl::SpinLock> lock(control_->mutex);
                id = control_->nextId++;
                control_->callbacks.push_back(std::make_pair(id,
                            std::forward<TCallback>(callback)));
            }

            return Connection::on_disconnect([id, control]() mutable
            {
                if (auto p = control.lock())
                {
                    std::unique_lock<btl::SpinLock> lock(p->mutex);
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

    template <typename T>
    class Collect2
    {
    public:
        struct Control
        {
            std::mutex mutex;
            std::vector<T> newValues;
            std::vector<T> values;
            std::vector<std::pair<uint64_t, std::function<void()>>> callbacks;
            uint64_t nextId = 1;
        };

        struct DataType
        {
            std::shared_ptr<Control> control;
            Stream<bool> stream;
        };

        Collect2(Stream<T> stream) :
            stream_(std::move(stream).share())
        {
        }

        DataType initialize() const
        {
            auto control = std::make_shared<Control>();
            return {
                control,
                stream_.fmap([control](auto value)
                    {
                        decltype(control->callbacks) callbacks;

                        {
                            std::unique_lock lock(control->mutex);
                            control->newValues.push_back(std::move(value));
                            std::swap(callbacks, control->callbacks);
                        }

                        for (auto&& cb : callbacks)
                            std::move(cb.second)();

                        return true;
                    })
            };
        }

        signal2::SignalResult<std::vector<T> const&> evaluate(
                DataType const& data) const
        {
            return signal2::SignalResult<std::vector<T> const&>(
                    data.control->values
                    );
        }

        bool hasChanged(DataType const& data) const
        {
            return !data.control->values.empty();
        }

        signal2::UpdateResult update(DataType& data, signal2::FrameInfo const&)
        {
            std::unique_lock lock(data.control->mutex);

            data.control->values.clear();
            std::swap(data.control->values, data.control->newValues);

            return { {}, !data.control->values.empty() };
        }

        Connection observe(DataType& data, std::function<void()> callback)
        {
            std::unique_lock lock(data.control->mutex);
            auto id = data.control->nextId++;

            data.control->callbacks.emplace_back(id, callback);

            std::weak_ptr<Control> weakControl = data.control;

            return Connection::on_disconnect([id, weakControl]()
                {
                    if (auto control = weakControl.lock())
                    {
                        for (auto i = control->callbacks.begin();
                                i != control->callbacks.end(); ++i)
                        {
                            if (id == i->first)
                            {
                                control->callbacks.erase(i);
                                break;
                            }
                        }
                    }
                });
        }

    private:
        SharedStream<T> stream_;
    };

    template <typename T>
    signal2::Signal<Collect2<T>, std::vector<T>> collect2(Stream<T> stream)
    {
        return signal2::wrap(Collect2<T>(std::move(stream)));
    }
} // reactive::stream

namespace reactive::signal2
{
    template <typename T>
    struct IsSignal<stream::Collect2<T>> : std::true_type {};
} // namespace reactive::signal2


