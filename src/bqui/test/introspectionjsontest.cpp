#include "agent/introspectionjson.h"

#include <bqui/widget/introspection.h>
#include <bqui/widget/datavalue.h>

#include <avg/obb.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

using namespace bqui;
using namespace bqui::widget;

using nlohmann::json;

TEST(introspectionJson, serialisesRoleNameAndCapabilities)
{
    Introspection node;
    node.name = "saveButton";
    node.role = "Button";
    node.capabilities = { Capability::Clickable, Capability::Focusable };
    node.obb = avg::Obb(avg::Vector2f(60.0f, 24.0f),
            avg::translate(70.0f, 12.0f));

    auto j = agent::toJson(node);

    EXPECT_EQ("Button", j.at("role"));
    EXPECT_EQ("saveButton", j.at("name"));
    EXPECT_EQ(json({ "Clickable", "Focusable" }), j.at("capabilities"));
    EXPECT_TRUE(j.at("children").is_array());
    EXPECT_TRUE(j.at("children").empty());
}

TEST(introspectionJson, omitsNameWhenUnset)
{
    Introspection node;
    node.role = "Widget";

    auto j = agent::toJson(node);

    EXPECT_FALSE(j.contains("name"));
    EXPECT_EQ("Widget", j.at("role"));
}

TEST(introspectionJson, encodesObbAsCenterSizeAngle)
{
    // A 40x20 box centred at (60,20): center is translation + half-size.
    Introspection node;
    node.role = "Box";
    node.obb = avg::Obb(avg::Vector2f(40.0f, 20.0f),
            avg::translate(40.0f, 10.0f));

    auto j = agent::toJson(node);

    auto const& obb = j.at("obb");
    EXPECT_DOUBLE_EQ(60.0, obb.at("center").at("x").get<double>());
    EXPECT_DOUBLE_EQ(20.0, obb.at("center").at("y").get<double>());
    EXPECT_DOUBLE_EQ(40.0, obb.at("size").at("w").get<double>());
    EXPECT_DOUBLE_EQ(20.0, obb.at("size").at("h").get<double>());
    EXPECT_DOUBLE_EQ(0.0, obb.at("angle").get<double>());
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

    auto j = agent::toJson(node);
    auto const& data = j.at("data");

    EXPECT_EQ("hi", data.at("text"));
    EXPECT_DOUBLE_EQ(3.0, data.at("count").get<double>());
    EXPECT_EQ(true, data.at("on").get<bool>());
    EXPECT_EQ("v", data.at("nested").at("k"));

    auto const& list = data.at("list");
    ASSERT_TRUE(list.is_array());
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ("a", list.at(0));
    EXPECT_DOUBLE_EQ(2.0, list.at(1).get<double>());
    EXPECT_EQ(false, list.at(2).get<bool>());
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

    auto j = agent::toJson(parent);

    EXPECT_EQ("CheckBoxLabel", j.at("role"));
    ASSERT_EQ(1u, j.at("children").size());

    auto const& childJson = j.at("children").at(0);
    EXPECT_EQ("Label", childJson.at("role"));
    EXPECT_EQ("Accept", childJson.at("data").at("text"));
}
