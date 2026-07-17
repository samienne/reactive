#include <bqui/widget/introspection.h>
#include <bqui/widget/datavalue.h>
#include <bqui/widget/widget.h>
#include <bqui/widget/label.h>
#include <bqui/widget/button.h>
#include <bqui/widget/textedit.h>
#include <bqui/widget/hbox.h>

#include <bqui/modifier/setwidgetintrospection.h>
#include <bqui/modifier/setsize.h>
#include <bqui/modifier/onclick.h>

#include <bqui/shape/shape.h>
#include <bqui/shape/rectangle.h>
#include <bqui/buildparams.h>

#include <avg/color.h>

#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <gtest/gtest.h>

#include <algorithm>

using namespace bq;
using namespace bqui;
using namespace bqui::widget;
using namespace bqui::modifier;

namespace
{
    // Build a widget, realise it at a concrete size, and read its introspection
    // tree. The obbs are realised geometry, so a size must be driven.
    Introspection introspect(AnyWidget widget,
            avg::Vector2f size = avg::Vector2f(200.0f, 100.0f))
    {
        auto sig = std::move(widget)(BuildParams{})(bq::signal::constant(size))
                .getIntrospection();
        return bq::signal::makeSignalContext(std::move(sig))
            .evaluate<0>().get<0>();
    }

    bool hasCapability(Introspection const& node, Capability cap)
    {
        return std::find(node.capabilities.begin(), node.capabilities.end(), cap)
            != node.capabilities.end();
    }

    std::string const* getText(Introspection const& node)
    {
        auto it = node.data.find("text");
        if (it == node.data.end())
            return nullptr;
        return std::get_if<std::string>(&it->second.value);
    }

    AnyWidget filledRect()
    {
        return shape::rectangle().fill(avg::Color(0.0f, 0.0f, 0.0f, 1.0f));
    }
} // namespace

TEST(introspection, labelSelfDescribes)
{
    auto node = introspect(label("Hello"));

    EXPECT_EQ("Label", node.role);
    ASSERT_NE(nullptr, getText(node));
    EXPECT_EQ("Hello", *getText(node));
}

TEST(introspection, buttonSelfDescribes)
{
    auto node = introspect(button("Save",
                bq::signal::constant<std::function<void()>>([]{})));

    EXPECT_EQ("Button", node.role);
    EXPECT_TRUE(hasCapability(node, Capability::Clickable));
    ASSERT_NE(nullptr, getText(node));
    EXPECT_EQ("Save", *getText(node));
}

TEST(introspection, textEditSelfDescribes)
{
    auto state = bq::signal::makeInput(TextEditState{"abc"});
    auto node = introspect(textEdit(state.handle, state.signal));

    EXPECT_EQ("TextEdit", node.role);
    EXPECT_TRUE(hasCapability(node, Capability::Editable));
    EXPECT_TRUE(hasCapability(node, Capability::Focusable));
    ASSERT_NE(nullptr, getText(node));
    EXPECT_EQ("abc", *getText(node));
}

TEST(introspection, onClickContributesClickable)
{
    auto widget = filledRect()
        | onClick(1, [](ClickEvent const&){});

    auto node = introspect(std::move(widget));

    EXPECT_TRUE(hasCapability(node, Capability::Clickable));
}

TEST(introspection, capabilitiesAccumulateAndDedup)
{
    auto node = introspect(makeWidget()
            | addCapability(Capability::Clickable)
            | addCapability(Capability::Focusable)
            | addCapability(Capability::Clickable));

    EXPECT_TRUE(hasCapability(node, Capability::Clickable));
    EXPECT_TRUE(hasCapability(node, Capability::Focusable));
    EXPECT_EQ(2u, node.capabilities.size());
}

TEST(introspection, setDataOverridesSingleKey)
{
    auto node = introspect(makeWidget()
            | setData("a", DataValue(1.0))
            | setData("b", DataValue(std::string("x")))
            | setData("a", DataValue(2.0)));

    ASSERT_EQ(1u, node.data.count("a"));
    ASSERT_EQ(1u, node.data.count("b"));
    EXPECT_EQ(DataValue(2.0), node.data.at("a"));
    EXPECT_EQ(DataValue(std::string("x")), node.data.at("b"));
}

TEST(introspection, roleAndNameTransformNotReplace)
{
    auto node = introspect(makeWidget()
            | setData("k", DataValue(true))
            | setRole("Custom")
            | setName("widget"));

    EXPECT_EQ("Custom", node.role);
    ASSERT_TRUE(node.name.has_value());
    EXPECT_EQ("widget", *node.name);
    // setRole/setName leave existing data untouched.
    ASSERT_EQ(1u, node.data.count("k"));
    EXPECT_EQ(DataValue(true), node.data.at("k"));
}

TEST(introspection, childrenCarryOwnDivergentObb)
{
    // A composite whose two children have different geometry, one of them
    // clickable. Models the CheckBoxLabel case: the clickable affordance is a
    // child node whose obb differs from its sibling and from the parent.
    auto small = filledRect()
        | setSize(avg::Vector2f(20.0f, 20.0f))
        | onClick(1, [](ClickEvent const&){})
        | setRole("Box");

    auto wide = filledRect()
        | setSize(avg::Vector2f(100.0f, 20.0f))
        | setRole("Filler");

    std::vector<AnyWidget> children;
    children.push_back(std::move(small));
    children.push_back(std::move(wide));

    auto node = introspect(hbox(std::move(children)) | setRole("CheckBoxLabel"));

    EXPECT_EQ("CheckBoxLabel", node.role);
    ASSERT_EQ(2u, node.children.size());

    // Find the clickable child; it must carry Clickable and its own obb.
    auto clickable = std::find_if(node.children.begin(), node.children.end(),
            [](Introspection const& c)
            {
                return hasCapability(c, Capability::Clickable);
            });
    ASSERT_NE(node.children.end(), clickable);
    EXPECT_EQ("Box", clickable->role);

    // The two children have divergent geometry (different obbs).
    EXPECT_NE(node.children[0].obb, node.children[1].obb);
}

TEST(introspection, leafObbIsRealisedSize)
{
    // A bare widget's obb is exactly the driven (realised) size.
    auto node = introspect(makeWidget() | setRole("Bare"),
            avg::Vector2f(320.0f, 240.0f));

    EXPECT_EQ(avg::Vector2f(320.0f, 240.0f), node.obb.getSize());
}

TEST(introspection, obbTracksRealisedSizeNotNatural)
{
    // A filled rectangle stretches to fill; its realised obb must follow the
    // driven window size, proving the obb is realised geometry, not a fixed
    // natural/hint size.
    auto small = introspect(filledRect() | setRole("Fill"),
            avg::Vector2f(100.0f, 50.0f));
    auto large = introspect(filledRect() | setRole("Fill"),
            avg::Vector2f(900.0f, 700.0f));

    EXPECT_EQ(avg::Vector2f(100.0f, 50.0f), small.obb.getSize());
    EXPECT_EQ(avg::Vector2f(900.0f, 700.0f), large.obb.getSize());
    EXPECT_NE(small.obb, large.obb);
}

TEST(introspection, stretchedChildObbExceedsNatural)
{
    // A stretchy child (filled rect, ~zero natural size) placed in an hbox that
    // is realised much larger than natural: its child obb must reflect the
    // stretched bounds, not the small natural size.
    std::vector<AnyWidget> children;
    children.push_back(filledRect() | setRole("Stretchy"));

    auto node = introspect(hbox(std::move(children)),
            avg::Vector2f(600.0f, 400.0f));

    ASSERT_EQ(1u, node.children.size());
    auto childSize = node.children[0].obb.getSize();
    EXPECT_GT(childSize[0], 100.0f);
    EXPECT_GT(childSize[1], 100.0f);
}

TEST(introspection, childObbIsWindowSpace)
{
    // Two fixed-size children in an hbox: the second child's obb must be offset
    // into window space by the first child's width (composed placement
    // transform), not sitting at the origin in its own local space.
    std::vector<AnyWidget> children;
    children.push_back(filledRect() | setSize(avg::Vector2f(80.0f, 40.0f))
            | setRole("First"));
    children.push_back(filledRect() | setSize(avg::Vector2f(80.0f, 40.0f))
            | setRole("Second"));

    auto node = introspect(hbox(std::move(children)),
            avg::Vector2f(400.0f, 40.0f));

    ASSERT_EQ(2u, node.children.size());

    auto first = node.children[0].obb.getCenter();
    auto second = node.children[1].obb.getCenter();

    // The second child sits to the right of the first (larger x center).
    EXPECT_GT(second[0], first[0]);
    // And the first child is not at the window origin's local frame — its
    // center has a positive x offset (half its own width at least).
    EXPECT_GT(first[0], 0.0f);
}
