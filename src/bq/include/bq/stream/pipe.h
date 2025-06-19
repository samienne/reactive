#pragma once

#include "handle.h"
#include "stream.h"

namespace bq
{
    namespace stream
    {
        template <typename T>
        struct Pipe
        {
            Handle<T> handle;
            Stream<T> stream;
        };

        template <typename T>
        Pipe<T> pipe()
        {
            auto control = std::make_shared<Control<T>>();
            control->callback = [](T value){ return value; };

            return {
                Handle<T>(control),
                Stream<T>(control)
            };
        }
    }
}

