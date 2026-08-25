#include "remote/introspectionjson.h"

#include <bqui/widget/introspection.h>
#include <bqui/widget/datavalue.h>

#include <avg/rendertree/snapshot.h>
#include <avg/obb.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

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

    auto j = remote::toJson(node);

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

    auto j = remote::toJson(node);

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

    auto j = remote::toJson(node);

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

    auto j = remote::toJson(node);
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

    auto j = remote::toJson(parent);

    EXPECT_EQ("CheckBoxLabel", j.at("role"));
    ASSERT_EQ(1u, j.at("children").size());

    auto const& childJson = j.at("children").at(0);
    EXPECT_EQ("Label", childJson.at("role"));
    EXPECT_EQ("Accept", childJson.at("data").at("text"));
}

TEST(snapshotJson, envelopeCarriesVersionTimeAndObb)
{
    avg::Snapshot snapshot;
    snapshot.time = std::chrono::milliseconds(42);
    snapshot.obb = avg::Obb(avg::Vector2f(40.0f, 20.0f),
            avg::translate(40.0f, 10.0f));

    auto j = remote::toJson(snapshot);

    EXPECT_EQ(1, j.at("version").get<int>());
    EXPECT_EQ(42, j.at("time").get<int64_t>());

    auto const& obb = j.at("obb");
    EXPECT_DOUBLE_EQ(60.0, obb.at("center").at("x").get<double>());
    EXPECT_DOUBLE_EQ(20.0, obb.at("center").at("y").get<double>());
    EXPECT_DOUBLE_EQ(0.0, obb.at("angle").get<double>());

    EXPECT_TRUE(j.at("root").is_null());
}

TEST(snapshotJson, nodeCarriesTypeIdObbTextAndChildren)
{
    auto id = avg::UniqueId();

    avg::SnapshotNode child;
    child.type = "RectNode";
    child.obb = avg::Obb(avg::Vector2f(10.0f, 10.0f));

    avg::SnapshotNode root;
    root.type = "IdNode";
    root.id = id;
    root.obb = avg::Obb(avg::Vector2f(100.0f, 50.0f));
    root.text.push_back(avg::SnapshotText{ "Ok",
            avg::Obb(avg::Vector2f(20.0f, 10.0f)) });
    root.children.push_back(child);

    avg::Snapshot snapshot;
    snapshot.obb = avg::Obb(avg::Vector2f(300.0f, 50.0f));
    snapshot.root = std::move(root);

    auto j = remote::toJson(snapshot);
    auto const& r = j.at("root");

    EXPECT_EQ("IdNode", r.at("type"));
    EXPECT_EQ(id.getValue(), r.at("id").get<uint64_t>());

    ASSERT_EQ(1u, r.at("text").size());
    EXPECT_EQ("Ok", r.at("text").at(0).at("text"));
    EXPECT_TRUE(r.at("text").at(0).contains("obb"));

    ASSERT_EQ(1u, r.at("children").size());
    auto const& childJson = r.at("children").at(0);
    EXPECT_EQ("RectNode", childJson.at("type"));
    // A node without an id omits the field rather than emitting null.
    EXPECT_FALSE(childJson.contains("id"));
}

TEST(snapshotJson, leavingIsWrittenOnlyWhenTrue)
{
    avg::SnapshotNode node;
    node.type = "TransitionNode";
    node.obb = avg::Obb(avg::Vector2f(100.0f, 50.0f));
    node.leaving = true;

    avg::Snapshot leaving;
    leaving.obb = avg::Obb(avg::Vector2f(300.0f, 50.0f));
    leaving.root = node;

    auto jLeaving = remote::toJson(leaving);
    EXPECT_TRUE(jLeaving.at("root").at("leaving").get<bool>());

    node.leaving = false;
    avg::Snapshot staying;
    staying.obb = avg::Obb(avg::Vector2f(300.0f, 50.0f));
    staying.root = node;

    auto jStaying = remote::toJson(staying);
    EXPECT_FALSE(jStaying.at("root").contains("leaving"));
}

TEST(snapshotJson, writesBoxesResolvedRatherThanAuthored)
{
    avg::SnapshotNode root;
    root.type = "ContainerNode";
    // The authored size is 100x50 under a scale of two.
    root.obb = avg::Obb(avg::Vector2f(100.0f, 50.0f), avg::scale(2.0f));
    // A text run authored 20x10 under a scale of three.
    root.text.push_back(avg::SnapshotText{ "T",
            avg::Obb(avg::Vector2f(20.0f, 10.0f), avg::scale(3.0f)) });

    avg::Snapshot snapshot;
    // The envelope itself is authored 300x50 under a scale of two.
    snapshot.obb = avg::Obb(avg::Vector2f(300.0f, 50.0f), avg::scale(2.0f));
    snapshot.root = std::move(root);

    auto j = remote::toJson(snapshot);

    auto const& nodeSize = j.at("root").at("obb").at("size");
    EXPECT_DOUBLE_EQ(200.0, nodeSize.at("w").get<double>());
    EXPECT_DOUBLE_EQ(100.0, nodeSize.at("h").get<double>());

    auto const& textSize = j.at("root").at("text").at(0).at("obb").at("size");
    EXPECT_DOUBLE_EQ(60.0, textSize.at("w").get<double>());
    EXPECT_DOUBLE_EQ(30.0, textSize.at("h").get<double>());

    auto const& envelopeSize = j.at("obb").at("size");
    EXPECT_DOUBLE_EQ(600.0, envelopeSize.at("w").get<double>());
    EXPECT_DOUBLE_EQ(100.0, envelopeSize.at("h").get<double>());
}

TEST(snapshotJson, preservesTextAndFoldsNonFiniteToZero)
{
    avg::SnapshotNode root;
    root.type = "ContainerNode";
    // A non-finite dimension has no JSON literal; it must fold to zero.
    root.obb = avg::Obb(avg::Vector2f(
                std::numeric_limits<float>::quiet_NaN(), 50.0f));
    root.text.push_back(avg::SnapshotText{ "a\"b\\d\nc\x01",
            avg::Obb(avg::Vector2f(10.0f, 10.0f)) });

    avg::Snapshot snapshot;
    snapshot.obb = avg::Obb(avg::Vector2f(300.0f, 50.0f));
    snapshot.root = std::move(root);

    auto j = remote::toJson(snapshot);

    EXPECT_EQ(std::string("a\"b\\d\nc\x01"),
            j.at("root").at("text").at(0).at("text").get<std::string>());
    EXPECT_DOUBLE_EQ(0.0,
            j.at("root").at("obb").at("size").at("w").get<double>());

    auto dump = j.dump();
    EXPECT_EQ(std::string::npos, dump.find("nan"));
    EXPECT_EQ(std::string::npos, dump.find("inf"));
}
