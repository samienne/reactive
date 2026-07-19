#pragma once

#include "avg/avgvisibility.h"

#include <optional>
#include <chrono>
#include <memory>

namespace avg
{
    class RenderTreeNode;

    struct AVG_EXPORT UpdateResult
    {
        std::shared_ptr<RenderTreeNode> node;
        std::optional<std::chrono::milliseconds> nextUpdate;
    };

    AVG_EXPORT std::optional<std::chrono::milliseconds> earlier(
            std::optional<std::chrono::milliseconds> t1,
            std::optional<std::chrono::milliseconds> t2
            );
} // namespace avg
