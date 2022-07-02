#pragma once

#include "futurecontrol.h"
#include "futurebase.h"

namespace btl::future
{
    namespace detail
    {
        template <typename TData, typename... Ts>
        class ControlWithData final : public FutureControl<Ts...>
        {
        public:
            ControlWithData(TData data) :
                data(std::move(data))
            {
            }

            TData data;
        };
    } // namespace detail
} // namespace btl::future

