#include <bqui/app.h>
#include <bqui/window.h>
#include <bqui/widget/widget.h>
#include <bqui/widget/introspection.h>
#include <bqui/widget/datavalue.h>

#include <bqui/modifier/setwidgetintrospection.h>
#include <bqui/modifier/onclick.h>

#include <bqui/clickevent.h>

#include <bqui/shape/shape.h>
#include <bqui/shape/rectangle.h>

#include <bqui/agent/transport.h>
#include <bqui/agent/json.h>

#include <bq/signal/signal.h>
#include <bq/signal/input.h>

#include <ase/platform.h>
#include <ase/dummyplatform.h>
#include <avg/color.h>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#ifdef _WIN32
#   include <process.h>
#else
#   include <unistd.h>
#endif

using namespace bqui;
using namespace bqui::widget;
using namespace bqui::agent;

namespace
{
    int currentPid()
    {
#ifdef _WIN32
        return _getpid();
#else
        return getpid();
#endif
    }

    std::string uniqueEndpoint(std::string const& name)
    {
        auto tag = name + "-" + std::to_string(currentPid());
#ifdef _WIN32
        return "\\\\.\\pipe\\bqui-" + tag;
#else
        return "/tmp/bqui-" + tag + ".sock";
#endif
    }

    // A widget whose click count is surfaced in introspection data so the agent
    // can read it back. `state` is shared so the click handler can advance it
    // without evaluating a signal outside a context.
    AnyWidget counterWidget(bq::signal::InputHandle<int> handle,
            bq::signal::AnySignal<int> count,
            std::shared_ptr<int> state)
    {
        auto data = count.map([](int c)
                {
                    return DataValue(static_cast<double>(c));
                });

        return shape::rectangle().fill(avg::Color(0.0f, 0.0f, 0.0f, 1.0f))
            | modifier::onClick(1, [handle, state](ClickEvent const&) mutable
                {
                    handle.set(++(*state));
                })
            | modifier::setName("counter")
            | modifier::setData("count", std::move(data));
    }

    // Walk the resolved introspection tree for the node named "counter" and
    // return its data["count"], or -1 if not found.
    std::optional<double> findCount(JsonValue const& node)
    {
        auto name = node.find("name");
        if (name && name->asString() == "counter")
        {
            auto data = node.find("data");
            if (data)
                if (auto count = data->find("count"))
                    return count->asNumber();
        }

        auto children = node.find("children");
        if (children && children->isArray())
            for (auto const& child : children->asArray())
                if (auto found = findCount(child))
                    return found;

        return std::nullopt;
    }

    double readCount(std::string const& snapshot)
    {
        auto value = parseJson(snapshot);
        if (!value)
            return -1.0;

        auto introspection = value->find("introspection");
        if (!introspection)
            return -1.0;

        return findCount(*introspection).value_or(-1.0);
    }

    std::string clickStep(float x, float y)
    {
        auto point = "\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y);
        return std::string("{\"type\":\"step\",\"dt_us\":16667,\"inject\":[")
            + "{\"kind\":\"pointerButton\"," + point
            + ",\"button\":1,\"state\":\"down\"},"
            + "{\"kind\":\"pointerButton\"," + point
            + ",\"button\":1,\"state\":\"up\"}]}";
    }
} // namespace

TEST(agentControlLoop, observeClickObserveRoundTrip)
{
    auto endpoint = uniqueEndpoint("agentloop");
    auto listener = listen(endpoint);

    auto count = bq::signal::makeInput(0);
    auto state = std::make_shared<int>(0);

    std::thread appThread([&]
    {
        app()
            .platform(ase::makeDummyPlatform())
            .agentic(true)
            .agentEndpoint(endpoint)
            .windows({
                    window(bq::signal::constant<std::string>("Agent"),
                        counterWidget(count.handle, count.signal, state))
                    })
            .run();
    });

    auto agent = listener->accept();

    // Observe: the counter starts at 0.
    agent->send(R"({"type":"snapshot"})");
    auto snap0 = agent->receive();
    ASSERT_TRUE(snap0);
    EXPECT_EQ(0.0, readCount(*snap0));

    // Act: click the counter at the centre of its 100x40 box (the root layout
    // places a single child filling the window, so its centre is the window
    // centre; the box is stretched to fill, so any interior point hits it).
    agent->send(clickStep(400.0f, 300.0f));
    auto snap1 = agent->receive();
    ASSERT_TRUE(snap1);
    EXPECT_EQ(1.0, readCount(*snap1));

    // A second click bumps it again, proving the loop keeps state across steps.
    agent->send(clickStep(400.0f, 300.0f));
    auto snap2 = agent->receive();
    ASSERT_TRUE(snap2);
    EXPECT_EQ(2.0, readCount(*snap2));

    agent->send(R"({"type":"quit"})");
    appThread.join();
}

TEST(agentControlLoop, cleanCloseEndsTheLoop)
{
    auto endpoint = uniqueEndpoint("agentclose");
    auto listener = listen(endpoint);

    auto count = bq::signal::makeInput(0);
    auto state = std::make_shared<int>(0);

    std::thread appThread([&]
    {
        app()
            .platform(ase::makeDummyPlatform())
            .agentic(true)
            .agentEndpoint(endpoint)
            .windows({
                    window(bq::signal::constant<std::string>("Agent"),
                        counterWidget(count.handle, count.signal, state))
                    })
            .run();
    });

    auto agent = listener->accept();

    agent->send(R"({"type":"snapshot"})");
    ASSERT_TRUE(agent->receive());

    // Dropping the channel (no quit) must still end the loop and the app.
    agent.reset();
    appThread.join();
}
