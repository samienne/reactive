#pragma once

#include "stream/pipe.h"
#include "stream/stream.h"
#include "collection.h"

#include <btl/variant.h>

namespace reactive
{

    template <typename T>
    struct DataSource
    {
        struct Insert
        {
            T value;
            size_t id;
        };

        struct Update
        {
            T value;
            size_t id;
        };

        struct Erase
        {
            size_t id;
        };

        using Event = btl::variant<Insert, Update, Erase>;

        stream::Stream<Event> input;
        std::function<void()> initialize;
        Connection connection;
    };

} // namespace reactive

