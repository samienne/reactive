#include <bqui/agent/session.h>
#include <bqui/agent/transport.h>
#include <bqui/agent/json.h>

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

#include <bq/signal/signal.h>
#include <bq/signal/input.h>

#include <ase/platform.h>
#include <ase/dummyplatform.h>
#include <avg/color.h>

#include <gtest/gtest.h>

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
} // namespace

// --- Fake-window unit tests (no App) -------------------------------------

namespace
{
    // A test double recording what the session drives it with.
    class FakeAgentWindow : public AgentWindow
    {
    public:
        explicit FakeAgentWindow(std::string name) : name_(std::move(name)) {}

        void injectPointerButton(unsigned int, unsigned int, ase::Vector2f,
                ase::ButtonState) override { ++buttonInjects_; }
        void injectPointerMove(unsigned int, ase::Vector2f) override {}
        void injectHover(unsigned int, ase::Vector2f, bool) override {}
        void injectKey(ase::KeyState, ase::KeyCode, uint32_t,
                std::string) override {}
        void injectText(std::string text) override { text_ = std::move(text); }

        Introspection introspect() const override
        {
            Introspection node;
            node.name = name_;
            node.role = "Fake";
            return node;
        }

        void advance(std::chrono::microseconds dt) override
        {
            ++advances_;
            totalDt_ += dt;
        }

        int buttonInjects() const { return buttonInjects_; }
        int advances() const { return advances_; }
        std::string const& text() const { return text_; }

    private:
        std::string name_;
        int buttonInjects_ = 0;
        int advances_ = 0;
        std::chrono::microseconds totalDt_{0};
        std::string text_;
    };

    // Run a session over `fakes` on a background thread against a loopback
    // endpoint, returning the connected agent side plus the server thread.
    struct SessionFixture
    {
        std::unique_ptr<TransportListener> listener;
        std::unique_ptr<Transport> agent;
        std::thread serverThread;

        SessionFixture(std::string const& name,
                std::vector<AgentWindow*> windows)
        {
            auto endpoint = uniqueEndpoint(name);
            listener = listen(endpoint);

            serverThread = std::thread([this, windows]
                {
                    auto server = listener->accept();
                    runSession(windows, *server);
                });

            agent = connect(endpoint);
        }

        std::optional<JsonValue> request(std::string const& command)
        {
            agent->send(command);
            auto reply = agent->receive();
            if (!reply)
                return std::nullopt;
            return parseJson(*reply);
        }

        ~SessionFixture()
        {
            agent->send(R"({"type":"quit"})");
            serverThread.join();
        }
    };
} // namespace

TEST(session, snapshotReturnsAllWindowsWithoutAdvancing)
{
    FakeAgentWindow a("w0");
    FakeAgentWindow b("w1");
    SessionFixture s("snap", { &a, &b });

    auto reply = s.request(R"({"type":"snapshot"})");
    ASSERT_TRUE(reply && reply->isObject());
    EXPECT_EQ("snapshot", reply->find("type").value_or(JsonValue()).asString());

    auto windows = reply->find("windows");
    ASSERT_TRUE(windows && windows->isArray());
    ASSERT_EQ(2u, windows->asArray().size());

    auto const& w0 = windows->asArray()[0];
    auto const& w1 = windows->asArray()[1];
    EXPECT_EQ(0.0, w0.find("index").value_or(JsonValue()).asNumber(-1));
    EXPECT_EQ(1.0, w1.find("index").value_or(JsonValue()).asNumber(-1));
    EXPECT_EQ("w0", w0.find("introspection").value_or(JsonValue())
            .find("name").value_or(JsonValue()).asString());
    EXPECT_EQ("w1", w1.find("introspection").value_or(JsonValue())
            .find("name").value_or(JsonValue()).asString());

    EXPECT_EQ(0, a.advances());
    EXPECT_EQ(0, b.advances());
}

TEST(session, stepRoutesInjectByIndexAndAdvancesAllWindows)
{
    FakeAgentWindow a("w0");
    FakeAgentWindow b("w1");
    SessionFixture s("step", { &a, &b });

    // Inject targets window 1 only; the step advances the whole app.
    auto reply = s.request(
            R"({"type":"step","dt_us":16667,"inject":[)"
            R"({"kind":"pointerButton","window":1,"x":1,"y":2,"button":1,"state":"down"}]})");
    ASSERT_TRUE(reply);

    EXPECT_EQ(0, a.buttonInjects());
    EXPECT_EQ(1, b.buttonInjects());
    EXPECT_EQ(1, a.advances());
    EXPECT_EQ(1, b.advances());
}

TEST(session, injectDefaultsToWindowZero)
{
    FakeAgentWindow a("w0");
    FakeAgentWindow b("w1");
    SessionFixture s("default", { &a, &b });

    // No "window" field -> window 0.
    s.request(R"({"type":"step","dt_us":0,"inject":[)"
              R"({"kind":"pointerButton","x":1,"y":2}]})");

    EXPECT_EQ(1, a.buttonInjects());
    EXPECT_EQ(0, b.buttonInjects());
}

TEST(session, outOfRangeWindowIndexIsSkipped)
{
    FakeAgentWindow a("w0");
    FakeAgentWindow b("w1");
    SessionFixture s("oob", { &a, &b });

    auto reply = s.request(R"({"type":"step","dt_us":0,"inject":[)"
                           R"({"kind":"pointerButton","window":5,"x":1,"y":2}]})");
    ASSERT_TRUE(reply);

    // The stray inject hit no window, but the step still advanced both.
    EXPECT_EQ(0, a.buttonInjects());
    EXPECT_EQ(0, b.buttonInjects());
    EXPECT_EQ(1, a.advances());
    EXPECT_EQ(1, b.advances());
}

TEST(session, malformedAndUnknownCommandsGetAnError)
{
    FakeAgentWindow a("w0");
    SessionFixture s("errors", { &a });

    auto malformed = s.request("not json");
    ASSERT_TRUE(malformed);
    EXPECT_EQ("error", malformed->find("type").value_or(JsonValue()).asString());

    auto unknown = s.request(R"({"type":"frobnicate"})");
    ASSERT_TRUE(unknown);
    EXPECT_EQ("error", unknown->find("type").value_or(JsonValue()).asString());

    EXPECT_EQ(0, a.advances());
}

TEST(session, cleanCloseEndsTheSession)
{
    FakeAgentWindow a("w0");
    auto endpoint = uniqueEndpoint("close");
    auto listener = listen(endpoint);

    std::vector<AgentWindow*> windows{ &a };
    std::thread serverThread([&]
        {
            auto server = listener->accept();
            runSession(windows, *server);
        });

    auto agent = connect(endpoint);
    agent->send(R"({"type":"snapshot"})");
    ASSERT_TRUE(agent->receive());

    // Dropping the channel (no quit) must end runSession.
    agent.reset();
    serverThread.join();
}

// --- One real-App integration anchor -------------------------------------

namespace
{
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

    // Read the counter from the first window of a multi-window snapshot.
    double readCount(std::string const& snapshot)
    {
        auto value = parseJson(snapshot);
        if (!value)
            return -1.0;

        auto windows = value->find("windows");
        if (!windows || !windows->isArray() || windows->asArray().empty())
            return -1.0;

        auto introspection = windows->asArray()[0].find("introspection");
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

TEST(session, observeClickObserveRoundTripDrivesARealApp)
{
    auto endpoint = uniqueEndpoint("agentloop");
    auto listener = listen(endpoint);

    auto count = bq::signal::makeInput(0);
    auto state = std::make_shared<int>(0);

    std::thread appThread([&]
    {
        App()
            .platform(ase::makeDummyPlatform())
            .agentic(true)
            .agentEndpoint(endpoint)
            .addWindow(
                    window(bq::signal::constant<std::string>("Agent"),
                        counterWidget(count.handle, count.signal, state)))
            .run();
    });

    auto agent = listener->accept();

    // Observe: the counter starts at 0.
    agent->send(R"({"type":"snapshot"})");
    auto snap0 = agent->receive();
    ASSERT_TRUE(snap0);
    EXPECT_EQ(0.0, readCount(*snap0));

    // Act: click the widget centre (it fills the window), then observe again.
    agent->send(clickStep(400.0f, 300.0f));
    auto snap1 = agent->receive();
    ASSERT_TRUE(snap1);
    EXPECT_EQ(1.0, readCount(*snap1));

    // A second click bumps it again, proving state persists across steps.
    agent->send(clickStep(400.0f, 300.0f));
    auto snap2 = agent->receive();
    ASSERT_TRUE(snap2);
    EXPECT_EQ(2.0, readCount(*snap2));

    agent->send(R"({"type":"quit"})");
    appThread.join();
}
