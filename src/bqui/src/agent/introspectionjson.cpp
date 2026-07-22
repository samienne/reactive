#include "introspectionjson.h"

#include "bqui/widget/datavalue.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <variant>

namespace bqui::agent
{

namespace
{

// A non-finite double has no JSON literal; keep the output valid by folding it
// to 0 rather than emitting a document nlohmann would refuse to serialise.
double finiteOrZero(double value)
{
    return std::isfinite(value) ? value : 0.0;
}

nlohmann::json toJson(widget::DataValue const& value)
{
    return std::visit([](auto const& v) -> nlohmann::json
    {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, std::string>)
        {
            return v;
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return finiteOrZero(v);
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            return v;
        }
        else if constexpr (std::is_same_v<T, widget::DataMap>)
        {
            auto object = nlohmann::json::object();
            for (auto const& entry : v)
                object[entry.first] = toJson(entry.second);
            return object;
        }
        else if constexpr (std::is_same_v<T, widget::DataArray>)
        {
            auto array = nlohmann::json::array();
            for (auto const& element : v)
                array.push_back(toJson(element));
            return array;
        }
    }, value.value);
}

nlohmann::json toJson(avg::Obb const& obb)
{
    auto center = obb.getCenter();
    auto size = obb.getSize();

    return {
        { "center", {
            { "x", finiteOrZero(center[0]) },
            { "y", finiteOrZero(center[1]) },
        } },
        { "size", {
            { "w", finiteOrZero(size[0]) },
            { "h", finiteOrZero(size[1]) },
        } },
        { "angle", finiteOrZero(obb.getTransform().getRotation()) },
    };
}

} // namespace

nlohmann::json toJson(widget::Introspection const& node)
{
    nlohmann::json result = nlohmann::json::object();

    if (node.name)
        result["name"] = *node.name;

    result["role"] = node.role;

    auto capabilities = nlohmann::json::array();
    for (auto capability : node.capabilities)
        capabilities.push_back(widget::toString(capability));
    result["capabilities"] = std::move(capabilities);

    result["obb"] = toJson(node.obb);

    auto data = nlohmann::json::object();
    for (auto const& entry : node.data)
        data[entry.first] = toJson(entry.second);
    result["data"] = std::move(data);

    auto children = nlohmann::json::array();
    for (auto const& child : node.children)
        children.push_back(toJson(*child));
    result["children"] = std::move(children);

    return result;
}

} // namespace bqui::agent
