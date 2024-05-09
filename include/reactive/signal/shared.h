#pragma once

#include "signalresult.h"
#include "updateresult.h"
#include "frameinfo.h"
#include "signaltraits.h"
#include "sharedcontrol.h"
#include "weak.h"

#include <btl/future/future.h>

#include <btl/async.h>
#include <btl/connection.h>
#include <btl/shared.h>

namespace reactive::signal
{
    template <typename TStorage, typename... Ts>
    class Shared
    {
    public:
        using Impl = SharedControl<TStorage, Ts...>;

        using DataType = typename Impl::DataType;

        Shared(TStorage sig) :
            Shared(std::make_shared<Impl>(std::move(sig)))
        {
        }

        Shared(btl::shared<Impl> impl) :
            impl_(std::move(impl))
        {
        }

        Shared(Shared const&) = default;
        Shared(Shared&&) noexcept = default;

        Shared& operator=(Shared const&) = default;
        Shared& operator=(Shared&&) noexcept = default;

        DataType initialize() const
        {
            return impl_->initialize();
        }

        bool hasChanged(DataType const& data) const
        {
            return impl_->hasChanged(data);
        }

        SignalResult<Ts const&...> evaluate(DataType const& data) const
        {
            return impl_->evaluate(data);
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            return impl_->update(data, frame);
        }

        template <typename TCallback>
        btl::connection observe(DataType& data, TCallback&& callback)
        {
            return impl_->observe(data, callback);
        }

        Signal<Weak<Ts...>, Ts...> weak() const
        {
            return wrap(Weak<Ts...>(std::make_shared<
                        WeakControl<Impl, Ts...>
                        >(impl_.ptr())));
        }

    private:
        btl::shared<Impl> impl_;
    };

    template <typename... Ts>
    struct IsSignal<Shared<Ts...>> : std::true_type {};
} // namespace reactive::signal

