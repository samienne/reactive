#pragma once

#include "futurebase.h"

namespace btl
{
    namespace future
    {
        namespace detail
        {
            template <typename T, typename TData>
            class ControlWithData final : public FutureControl<std::decay_t<T>>
            {
            public:
                ControlWithData(TData data) :
                    data(std::move(data))
                {
                }

                TData data;
            };
        } // detail
    } // future
} // btl

