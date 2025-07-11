#pragma once

#include "connection.h"

#include <bq/stream/stream.h>

#include <variant>

namespace bqui
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

        bq::stream::Stream<Event> input;
        std::function<std::vector<std::pair<std::size_t, T>>()> evaluate;
        Connection connection;
    };

}

