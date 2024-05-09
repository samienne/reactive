#pragma once

#include "stream.h"
#include "sharedstream.h"

#include <reactive/signal/signalresult.h>
#include <reactive/signal/updateresult.h>
#include <reactive/signal/frameinfo.h>
#include <reactive/signal/signal.h>

#include <btl/demangle.h>
#include <btl/spinlock.h>
#include <btl/shared.h>

#include <mutex>

namespace reactive::stream
{
    template <typename T>
    class Collect
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

        Collect(Stream<T> stream) :
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

        signal::SignalResult<std::vector<T> const&> evaluate(
                DataType const& data) const
        {
            return signal::SignalResult<std::vector<T> const&>(
                    data.control->values
                    );
        }

        bool hasChanged(DataType const& data) const
        {
            return !data.control->values.empty();
        }

        signal::UpdateResult update(DataType& data, signal::FrameInfo const&)
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
    signal::Signal<Collect<T>, std::vector<T>> collect(Stream<T> stream)
    {
        return signal::wrap(Collect<T>(std::move(stream)));
    }
} // reactive::stream

namespace reactive::signal
{
    template <typename T>
    struct IsSignal<stream::Collect<T>> : std::true_type {};
} // namespace reactive::signal

