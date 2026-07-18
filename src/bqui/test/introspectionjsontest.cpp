#include <bqui/agent/introspectionjson.h>
#include <bqui/widget/introspection.h>
#include <bqui/widget/datavalue.h>

#include <avg/obb.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <gtest/gtest.h>

#include <string>

using namespace bqui;
using namespace bqui::widget;

namespace
{
    bool contains(std::string const& haystack, std::string const& needle)
    {
        return haystack.find(needle) != std::string::npos;
    }
} // namespace

TEST(introspectionJson, serialisesRoleNameAndCapabilities)
{
    Introspection node;
    node.name = "saveButton";
    node.role = "Button";
    node.capabilities = { Capability::Clickable, Capability::Focusable };
    node.obb = avg::Obb(avg::Vector2f(60.0f, 24.0f),
            avg::translate(70.0f, 12.0f));

    auto json = agent::toJson(node);

    EXPECT_TRUE(contains(json, "\"role\":\"Button\""));
    EXPECT_TRUE(contains(json, "\"name\":\"saveButton\""));
    EXPECT_TRUE(contains(json, "\"capabilities\":[\"Clickable\",\"Focusable\"]"));
    EXPECT_TRUE(contains(json, "\"children\":[]"));
}

TEST(introspectionJson, omitsNameWhenUnset)
{
    Introspection node;
    node.role = "Widget";

    auto json = agent::toJson(node);

    EXPECT_FALSE(contains(json, "\"name\""));
    EXPECT_TRUE(contains(json, "\"role\":\"Widget\""));
}

TEST(introspectionJson, encodesObbAsCenterSizeAngle)
{
    // A 40x20 box centred at (60,20): center is translation + half-size.
    Introspection node;
    node.role = "Box";
    node.obb = avg::Obb(avg::Vector2f(40.0f, 20.0f),
            avg::translate(40.0f, 10.0f));

    auto json = agent::toJson(node);

    EXPECT_TRUE(contains(json, "\"obb\":{"));
    EXPECT_TRUE(contains(json, "\"center\":{\"x\":60,\"y\":20}"));
    EXPECT_TRUE(contains(json, "\"size\":{\"w\":40,\"h\":20}"));
    EXPECT_TRUE(contains(json, "\"angle\":0"));
}

TEST(introspectionJson, serialisesNestedDataObjectAndArray)
{
    Introspection node;
    node.role = "Widget";
    node.data["text"] = DataValue(std::string("hi"));
    node.data["count"] = DataValue(3.0);
    node.data["on"] = DataValue(true);

    DataMap nested;
    nested["k"] = DataValue(std::string("v"));
    node.data["nested"] = DataValue(std::move(nested));

    node.data["list"] = DataValue(DataArray{
            DataValue(std::string("a")),
            DataValue(2.0),
            DataValue(false)
            });

    auto json = agent::toJson(node);

    EXPECT_TRUE(contains(json, "\"text\":\"hi\""));
    EXPECT_TRUE(contains(json, "\"count\":3"));
    EXPECT_TRUE(contains(json, "\"on\":true"));
    EXPECT_TRUE(contains(json, "\"nested\":{\"k\":\"v\"}"));
    EXPECT_TRUE(contains(json, "\"list\":[\"a\",2,false]"));
}

TEST(introspectionJson, escapesStringSpecialCharacters)
{
    Introspection node;
    node.role = "Widget";
    node.data["s"] = DataValue(std::string("a\"b\\c\n\t"));

    auto json = agent::toJson(node);

    EXPECT_TRUE(contains(json, "\"s\":\"a\\\"b\\\\c\\n\\t\""));
}

TEST(introspectionJson, recursesIntoChildren)
{
    Introspection child;
    child.role = "Label";
    child.data["text"] = DataValue(std::string("Accept"));

    Introspection parent;
    parent.role = "CheckBoxLabel";
    parent.capabilities = { Capability::Clickable };
    parent.children.push_back(makeIntrospectionChild(std::move(child)));

    auto json = agent::toJson(parent);

    EXPECT_TRUE(contains(json, "\"role\":\"CheckBoxLabel\""));
    EXPECT_TRUE(contains(json, "\"children\":[{"));
    EXPECT_TRUE(contains(json, "\"role\":\"Label\""));
    EXPECT_TRUE(contains(json, "\"text\":\"Accept\""));
    // No trailing comma before the closing of the children array.
    EXPECT_FALSE(contains(json, ",]"));
    EXPECT_FALSE(contains(json, ",}"));
}
