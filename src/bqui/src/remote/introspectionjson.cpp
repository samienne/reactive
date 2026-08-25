#include "introspectionjson.h"

#include "bqui/widget/datavalue.h"

#include <avg/rendertree/snapshot.h>

#include <nlohmann/json.hpp>

#include <cmath>
#include <utility>
#include <variant>

namespace bqui::remote
{

namespace
{

// A non-finite double has no JSON literal; fold it to 0 to keep the output
// valid, rather than the null nlohmann emits for it by default.
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

namespace
{

// The schema version of the render-tree JSON. Bump it when an existing field
// changes meaning; extend the schema additively otherwise.
constexpr int kRenderTreeSchemaVersion = 1;

// A snapshot node's box keeps its scale in the transform rather than in the
// stored size, so the resolved extent is the authored size times that scale.
nlohmann::json snapshotObbToJson(avg::Obb const& obb)
{
    auto center = obb.getCenter();
    auto scale = obb.getTransform().getScale();
    auto size = obb.getSize();

    return {
        { "center", {
            { "x", finiteOrZero(center[0]) },
            { "y", finiteOrZero(center[1]) },
        } },
        { "size", {
            { "w", finiteOrZero(size[0] * scale) },
            { "h", finiteOrZero(size[1] * scale) },
        } },
        { "angle", finiteOrZero(obb.getTransform().getRotation()) },
    };
}

nlohmann::json toJson(avg::SnapshotNode const& node)
{
    nlohmann::json result = nlohmann::json::object();

    result["type"] = node.type;

    if (node.id)
        result["id"] = node.id->getValue();

    result["obb"] = snapshotObbToJson(node.obb);

    if (node.leaving)
        result["leaving"] = true;

    auto text = nlohmann::json::array();
    for (auto const& entry : node.text)
        text.push_back({
            { "text", entry.text },
            { "obb", snapshotObbToJson(entry.obb) },
        });
    result["text"] = std::move(text);

    auto children = nlohmann::json::array();
    for (auto const& child : node.children)
        children.push_back(toJson(child));
    result["children"] = std::move(children);

    return result;
}

} // namespace

nlohmann::json toJson(avg::Snapshot const& snapshot)
{
    nlohmann::json result = nlohmann::json::object();

    result["version"] = kRenderTreeSchemaVersion;
    result["time"] = snapshot.time.count();
    result["obb"] = snapshotObbToJson(snapshot.obb);

    if (snapshot.root)
        result["root"] = toJson(*snapshot.root);
    else
        result["root"] = nullptr;

    return result;
}

} // namespace bqui::remote
