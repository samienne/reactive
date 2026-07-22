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

#include <btl/uniqueid.h>

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
    // A test double recording what the session drives it with. It carries a
    // distinct id and title so the session can address it on the wire.
    class FakeAgentWindow : public AgentWindow
    {
    public:
        FakeAgentWindow(btl::UniqueId id, std::string name)
            : id_(id), name_(std::move(name)) {}

        btl::UniqueId id() const override { return id_; }
        std::string title() const override { return name_; }

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
        btl::UniqueId id_;
        std::string name_;
        int buttonInjects_ = 0;
        int advances_ = 0;
        std::chrono::microseconds totalDt_{0};
        std::string text_;
    };

    // An AgentApp over a fixed window set: the reconcile is a no-op (the fakes
    // never open or close), and the live set is always the same fakes. Enough
    // to exercise the session's routing and advancing without a real App.
    AgentApp staticApp(AgentWindows windows)
    {
        AgentApp app;
        app.reconcile = [](std::chrono::microseconds) {};
        app.liveWindows = [windows] { return windows; };
        return app;
    }

    // Run a session over `fakes` on a background thread against a loopback
    // endpoint, returning the connected agent side plus the server thread.
    struct SessionFixture
    {
        std::unique_ptr<TransportListener> listener;
        std::unique_ptr<Transport> agent;
        std::thread serverThread;

        SessionFixture(std::string const& name, AgentWindows windows)
        {
            auto endpoint = uniqueEndpoint(name);
            listener = listen(endpoint);

            serverThread = std::thread([this, windows]
                {
                    auto server = listener->accept();
                    runSession(staticApp(windows), *server);
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
    auto idA = btl::makeUniqueId();
    auto idB = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    FakeAgentWindow b(idB, "w1");
    SessionFixture s("snap", { a, b });

    auto reply = s.request(R"({"type":"snapshot"})");
    ASSERT_TRUE(reply && reply->isObject());
    EXPECT_EQ("snapshot", reply->find("type").value_or(JsonValue()).asString());

    auto windows = reply->find("windows");
    ASSERT_TRUE(windows && windows->isArray());
    ASSERT_EQ(2u, windows->asArray().size());

    auto const& w0 = windows->asArray()[0];
    auto const& w1 = windows->asArray()[1];
    EXPECT_EQ(static_cast<double>(idA.getValue()),
            w0.find("id").value_or(JsonValue()).asNumber(-1));
    EXPECT_EQ(static_cast<double>(idB.getValue()),
            w1.find("id").value_or(JsonValue()).asNumber(-1));
    EXPECT_EQ("w0", w0.find("title").value_or(JsonValue()).asString());
    EXPECT_EQ("w1", w1.find("title").value_or(JsonValue()).asString());
    EXPECT_EQ("w0", w0.find("introspection").value_or(JsonValue())
            .find("name").value_or(JsonValue()).asString());
    EXPECT_EQ("w1", w1.find("introspection").value_or(JsonValue())
            .find("name").value_or(JsonValue()).asString());

    EXPECT_EQ(0, a.advances());
    EXPECT_EQ(0, b.advances());
}

TEST(session, stepRoutesInjectByIdAndAdvancesAllWindows)
{
    auto idA = btl::makeUniqueId();
    auto idB = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    FakeAgentWindow b(idB, "w1");
    SessionFixture s("step", { a, b });

    // Inject targets window b by id only; the step advances the whole app.
    auto reply = s.request(
            R"({"type":"step","dt_us":16667,"inject":[)"
            R"({"kind":"pointerButton","window":)"
            + std::to_string(idB.getValue())
            + R"(,"x":1,"y":2,"button":1,"state":"down"}]})");
    ASSERT_TRUE(reply);

    EXPECT_EQ(0, a.buttonInjects());
    EXPECT_EQ(1, b.buttonInjects());
    EXPECT_EQ(1, a.advances());
    EXPECT_EQ(1, b.advances());
}

TEST(session, injectDefaultsToFirstWindow)
{
    auto idA = btl::makeUniqueId();
    auto idB = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    FakeAgentWindow b(idB, "w1");
    SessionFixture s("default", { a, b });

    // No "window" field -> the first window.
    s.request(R"({"type":"step","dt_us":0,"inject":[)"
              R"({"kind":"pointerButton","x":1,"y":2}]})");

    EXPECT_EQ(1, a.buttonInjects());
    EXPECT_EQ(0, b.buttonInjects());
}

TEST(session, unknownWindowIdIsSkipped)
{
    auto idA = btl::makeUniqueId();
    auto idB = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    FakeAgentWindow b(idB, "w1");
    SessionFixture s("oob", { a, b });

    // An id that names no live window (never assigned to either fake).
    auto strayId = btl::makeUniqueId();
    auto reply = s.request(R"({"type":"step","dt_us":0,"inject":[)"
                           R"({"kind":"pointerButton","window":)"
                           + std::to_string(strayId.getValue())
                           + R"(,"x":1,"y":2}]})");
    ASSERT_TRUE(reply);

    // The stray inject hit no window, but the step still advanced both.
    EXPECT_EQ(0, a.buttonInjects());
    EXPECT_EQ(0, b.buttonInjects());
    EXPECT_EQ(1, a.advances());
    EXPECT_EQ(1, b.advances());
}

TEST(session, malformedAndUnknownCommandsGetAnError)
{
    FakeAgentWindow a(btl::makeUniqueId(), "w0");
    SessionFixture s("errors", { a });

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
    FakeAgentWindow a(btl::makeUniqueId(), "w0");
    auto endpoint = uniqueEndpoint("close");
    auto listener = listen(endpoint);

    AgentWindows windows{ a };
    std::thread serverThread([&]
        {
            auto server = listener->accept();
            runSession(staticApp(windows), *server);
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

// --- Dynamic-windows integration: the window set is live -----------------

namespace
{
    // The "windows" array of a snapshot (empty if the snapshot is malformed).
    JsonArray snapshotWindows(std::string const& snapshot)
    {
        auto value = parseJson(snapshot);
        if (!value)
            return {};

        auto windows = value->find("windows");
        if (!windows || !windows->isArray())
            return {};

        return windows->asArray();
    }

    // The snapshot entry whose title matches, or nullopt.
    std::optional<JsonValue> windowByTitle(std::string const& snapshot,
            std::string const& title)
    {
        for (auto const& w : snapshotWindows(snapshot))
            if (w.find("title").value_or(JsonValue()).asString() == title)
                return w;

        return std::nullopt;
    }

    // Whether an introspection subtree carries a node with the given name.
    bool treeHasName(JsonValue const& node, std::string const& name)
    {
        if (node.find("name").value_or(JsonValue()).asString() == name)
            return true;

        auto children = node.find("children");
        if (children && children->isArray())
            for (auto const& child : children->asArray())
                if (treeHasName(child, name))
                    return true;

        return false;
    }

    // A window whose introspection is present and carries the named node —
    // proof the tree is non-empty and belongs to that window.
    bool introspectionHasName(JsonValue const& window, std::string const& name)
    {
        auto tree = window.find("introspection");
        return tree && tree->isObject() && treeHasName(*tree, name);
    }

    // A click (down then up) at a point, routed to a specific window by id.
    std::string clickWindowStep(uint64_t id, float x, float y)
    {
        auto point = "\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y);
        auto win = "\"window\":" + std::to_string(id);
        return std::string("{\"type\":\"step\",\"dt_us\":16667,\"inject\":[")
            + "{\"kind\":\"pointerButton\"," + win + "," + point
            + ",\"button\":1,\"state\":\"down\"},"
            + "{\"kind\":\"pointerButton\"," + win + "," + point
            + ",\"button\":1,\"state\":\"up\"}]}";
    }

    uint64_t wireId(JsonValue const& window)
    {
        return static_cast<uint64_t>(
                window.find("id").value_or(JsonValue()).asNumber());
    }
} // namespace

// A window opened by a click is visible in the same step's snapshot, and a
// window it closes is gone from the next — the agent sees a live set, keyed by
// id and title rather than a frozen array index.
TEST(session, dynamicWindowsOpenAndCloseDuringASession)
{
    auto endpoint = uniqueEndpoint("agentdynamic");
    auto listener = listen(endpoint);

    App app;
    app.platform(ase::makeDummyPlatform())
        .agentic(true)
        .agentEndpoint(endpoint);

    // The child closes itself on a click. Its id is known only once it exists,
    // so the handler reads it through a slot filled after construction.
    auto childId = std::make_shared<std::optional<btl::UniqueId>>();
    App childApp = app;
    Window child = window(
            bq::signal::constant<std::string>("child"),
            shape::rectangle().fill(avg::Color(0.2f, 0.2f, 0.2f, 1.0f))
                | modifier::onClick(1,
                        [childApp, childId](ClickEvent const&) mutable
                        {
                            if (*childId)
                                childApp.removeWindow(**childId);
                        })
                | modifier::setName("childRoot"));
    *childId = child.getId();

    // The main window opens the child on a click, once.
    App mainApp = app;
    auto opened = std::make_shared<bool>(false);
    Window main = window(
            bq::signal::constant<std::string>("main"),
            shape::rectangle().fill(avg::Color(0.0f, 0.0f, 0.0f, 1.0f))
                | modifier::onClick(1,
                        [mainApp, child, opened](ClickEvent const&) mutable
                        {
                            if (!*opened)
                            {
                                *opened = true;
                                mainApp.addWindow(child);
                            }
                        })
                | modifier::setName("mainRoot"));

    app.addWindow(main);

    std::thread appThread([&] { app.run(); });

    auto agent = listener->accept();

    // Observe: exactly one window, "main".
    agent->send(R"({"type":"snapshot"})");
    auto snap0 = agent->receive();
    ASSERT_TRUE(snap0);
    ASSERT_EQ(1u, snapshotWindows(*snap0).size());
    auto mainEntry = windowByTitle(*snap0, "main");
    ASSERT_TRUE(mainEntry);
    uint64_t mainId = wireId(*mainEntry);

    // Act: click main's centre. Its handler opens the child, and the same
    // step's snapshot must already show both windows.
    agent->send(clickWindowStep(mainId, 400.0f, 300.0f));
    auto snap1 = agent->receive();
    ASSERT_TRUE(snap1);
    ASSERT_EQ(2u, snapshotWindows(*snap1).size());

    auto mainAfter = windowByTitle(*snap1, "main");
    auto childAfter = windowByTitle(*snap1, "child");
    ASSERT_TRUE(mainAfter);
    ASSERT_TRUE(childAfter);
    EXPECT_TRUE(introspectionHasName(*mainAfter, "mainRoot"));
    EXPECT_TRUE(introspectionHasName(*childAfter, "childRoot"));

    uint64_t childWireId = wireId(*childAfter);
    EXPECT_NE(mainId, childWireId);

    // Act: click the child's centre. Its handler closes it, and the next
    // snapshot is back to just "main".
    agent->send(clickWindowStep(childWireId, 400.0f, 300.0f));
    auto snap2 = agent->receive();
    ASSERT_TRUE(snap2);
    ASSERT_EQ(1u, snapshotWindows(*snap2).size());
    EXPECT_TRUE(windowByTitle(*snap2, "main"));
    EXPECT_FALSE(windowByTitle(*snap2, "child"));

    agent->send(R"({"type":"quit"})");
    appThread.join();
}
