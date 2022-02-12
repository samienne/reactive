#pragma once

#include "stream/pipe.h"
#include "stream/stream.h"
#include "collection.h"

#include <variant>

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

        struct Refresh
        {
            std::vector<std::pair<size_t, T>> values;
        };

        using Event = std::variant<Insert, Update, Erase, Swap, Move, Refresh>;

        stream::Stream<Event> input;
        std::function<void()> initialize;
        Connection connection;
    };

} // namespace reactive

