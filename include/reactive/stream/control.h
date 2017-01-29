#pragma once

#include <functional>

namespace reactive
{
    namespace stream
    {
        template <typename T>
        struct Control
        {
            virtual ~Control() {}

            std::function<void(T)> callback;
        };

        template <typename T, typename TData>
        struct ControlWithData : Control<T>
        {
            ControlWithData(TData&& data) :
                data(std::forward<TData>(data))
            {
            }

            TData data;
        };
    } // stream
} // reactive

