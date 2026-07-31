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

#include <cstdint>
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
            std::weak_ptr<signal::ObserveControl> observe;
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

        DataType initialize(signal::DataContext& context,
                signal::FrameInfo const&) const
        {
            auto control = std::make_shared<Control>();
            control->observe = context.observeControl();
            return {
                control,
                stream_.fmap([control](auto value)
                    {
                        std::weak_ptr<signal::ObserveControl> observe;
                        {
                            std::unique_lock<std::mutex> lock(control->mutex);
                            control->newValues.push_back(std::move(value));
                            observe = control->observe;
                        }
                        if (auto c = observe.lock())
                            c->fire();
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

