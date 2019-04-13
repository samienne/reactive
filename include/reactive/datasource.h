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
            int index;
            size_t id;
        };

        struct Update
        {
            T value;
            int index;
            size_t id;
        };

        struct Erase
        {
            size_t id;
        };

        struct Swap
        {
            size_t id1;
            int index1;
            size_t id2;
            int index2;
        };

        struct Move
        {
            size_t id;
            int newIndex;
        };

        using Event = btl::variant<Insert, Update, Erase, Swap, Move>;

        stream::Stream<Event> input;
        std::function<void()> initialize;
        Connection connection;
    };

} // namespace reactive

