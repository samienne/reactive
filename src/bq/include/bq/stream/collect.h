#pragma once

#include "stream.h"
#include "sharedstream.h"

#include <bq/signal/signalresult.h>
#include <bq/signal/updateresult.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/signal.h>
#include <bq/signal/datacontext.h>

#include <btl/demangle.h>
#include <btl/spinlock.h>
#include <btl/shared.h>

#include <mutex>

namespace bq::stream
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

        DataType initialize(signal::DataContext&, signal::FrameInfo const&) const
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
                signal::DataContext&, DataType const& data) const
        {
            return signal::SignalResult<std::vector<T> const&>(
                    data.control->values
                    );
        }

        signal::UpdateResult update(signal::DataContext&,
                DataType& data, signal::FrameInfo const&)
        {
            std::unique_lock lock(data.control->mutex);

            data.control->values.clear();
            std::swap(data.control->values, data.control->newValues);

            return { {}, !data.control->values.empty() };
        }

        btl::connection observe(signal::DataContext&, DataType& data,
                std::function<void()> callback)
        {
            std::unique_lock lock(data.control->mutex);
            auto id = data.control->nextId++;

            data.control->callbacks.emplace_back(id, callback);

            std::weak_ptr<Control> weakControl = data.control;

            return btl::connection::on_disconnect([id, weakControl]()
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

namespace bq::signal
{
    template <typename T>
    struct IsSignal<stream::Collect<T>> : std::true_type {};
} // namespace bq::signal

