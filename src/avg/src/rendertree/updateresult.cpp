#include "rendertree/updateresult.h"

#include <algorithm>
#include <chrono>
#include <optional>

namespace avg
{

std::optional<std::chrono::milliseconds> earlier(
        std::optional<std::chrono::milliseconds> t1,
        std::optional<std::chrono::milliseconds> t2
        )
{
    if (t1.has_value() && t2.has_value())
        return std::min(*t1, *t2);
    else if (t1.has_value())
        return t1;
    else
        return t2;
}

} // namespace avg
