#include <bqui/agent/json.h>

#include <gtest/gtest.h>

#include <string>

using namespace bqui::agent;

TEST(json, parsesObjectWithMixedValues)
{
    auto value = parseJson(
            R"({"type":"step","dt_us":16667,"ok":true,"note":"hi","none":null})");

    ASSERT_TRUE(value);
    ASSERT_TRUE(value->isObject());

    EXPECT_EQ("step", value->find("type").value_or(JsonValue()).asString());
    EXPECT_EQ(16667.0, value->find("dt_us").value_or(JsonValue()).asNumber());
    EXPECT_TRUE(value->find("ok").value_or(JsonValue()).asBool());
    EXPECT_EQ("hi", value->find("note").value_or(JsonValue()).asString());
    EXPECT_FALSE(value->find("missing").has_value());
}

TEST(json, parsesNestedArrayOfObjects)
{
    auto value = parseJson(
            R"({"inject":[{"kind":"pointerButton","x":10,"y":20},{"kind":"text"}]})");

    ASSERT_TRUE(value);
    auto inject = value->find("inject");
    ASSERT_TRUE(inject && inject->isArray());
    ASSERT_EQ(2u, inject->asArray().size());

    auto const& first = inject->asArray()[0];
    EXPECT_EQ("pointerButton", first.find("kind").value_or(JsonValue()).asString());
    EXPECT_EQ(10.0, first.find("x").value_or(JsonValue()).asNumber());
    EXPECT_EQ(20.0, first.find("y").value_or(JsonValue()).asNumber());
}

TEST(json, decodesStringEscapes)
{
    auto value = parseJson(R"({"s":"a\"b\\c\n\tA"})");

    ASSERT_TRUE(value);
    EXPECT_EQ("a\"b\\c\n\tA", value->find("s").value_or(JsonValue()).asString());
}

TEST(json, rejectsMalformedInput)
{
    EXPECT_FALSE(parseJson("{").has_value());
    EXPECT_FALSE(parseJson("{\"a\":}").has_value());
    EXPECT_FALSE(parseJson("[1,2").has_value());
    EXPECT_FALSE(parseJson("nul").has_value());
    EXPECT_FALSE(parseJson("{\"a\":1,}").has_value());
    EXPECT_FALSE(parseJson("").has_value());
    EXPECT_FALSE(parseJson("{}trailing").has_value());
}

TEST(json, rejectsPathologicallyDeepNesting)
{
    // Deep nesting must fail cleanly (no stack overflow).
    std::string deep(1000, '[');
    EXPECT_FALSE(parseJson(deep).has_value());
}

TEST(json, wrongTypeAccessorsReturnFallback)
{
    auto value = parseJson(R"({"n":5})");

    ASSERT_TRUE(value);
    // Reading a number as a string / bool yields the fallback, not a crash.
    EXPECT_EQ("", value->find("n").value_or(JsonValue()).asString());
    EXPECT_TRUE(value->find("n").value_or(JsonValue()).asBool(true));
}
