#pragma once

#include <functional>
#include <memory>

namespace bq
{
    namespace stream
    {
        template <typename T>
        struct SharedControl;

        template <typename T>
        struct Control
        {
            virtual ~Control() {}

            std::function<void(T)> callback;

            // The SharedControl produced by this control's first share(), kept
            // weakly. A later share() of the same underlying stream reuses this
            // broadcast instead of overwriting callback and starving it.
            std::weak_ptr<SharedControl<T>> shared;
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

