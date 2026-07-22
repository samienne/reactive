#include <bqui/agent/session.h>
#include <bqui/agent/transport.h>

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

#include <bqui/widget/label.h>

#include <bq/signal/signal.h>
#include <bq/signal/input.h>

#include <ase/platform.h>
#include <ase/dummyplatform.h>
#include <avg/color.h>
#include <avg/obb.h>
#include <avg/vector.h>
#include <avg/rendertree/snapshot.h>

#include <btl/uniqueid.h>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
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

using nlohmann::json;

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

    // Send one JSON-RPC request and read its reply, over a raw transport.
    json rpc(Transport& transport, int id, std::string const& method,
            json params = json::object())
    {
        json request = {
            { "jsonrpc", "2.0" },
            { "id", id },
            { "method", method },
        };
        if (!params.empty())
            request["params"] = params;

        transport.send(request.dump());
        auto reply = transport.receive();
        return reply ? json::parse(*reply) : json();
    }

    // Send a request without waiting for its reply — the caller reads it later
    // through awaitResponse, past any interleaved notifications.
    void sendRequest(Transport& transport, int id, std::string const& method,
            json params = json::object())
    {
        json request = {
            { "jsonrpc", "2.0" },
            { "id", id },
            { "method", method },
        };
        if (!params.empty())
            request["params"] = params;

        transport.send(request.dump());
    }

    // Read frames until the response with `id` arrives, tallying any `frame`
    // notifications seen on the way (they interleave with responses while the
    // app free-runs). Returns a null json if the channel closes first.
    json awaitResponse(Transport& transport, int id, int* frameNotifs = nullptr)
    {
        for (;;)
        {
            auto raw = transport.receive();
            if (!raw)
                return json();

            json msg = json::parse(*raw, nullptr, false);
            if (msg.is_discarded())
                continue;

            auto method = msg.find("method");
            if (method != msg.end() && *method == "frame")
            {
                if (frameNotifs)
                    ++*frameNotifs;
                continue;
            }

            auto msgId = msg.find("id");
            if (msgId != msg.end() && msgId->is_number()
                    && msgId->get<int>() == id)
                return msg;
        }
    }

    // Read frames until at least one `frame` notification arrives (proof the
    // app advanced a frame while running). Returns false if the channel closes.
    bool awaitFrameNotification(Transport& transport)
    {
        for (;;)
        {
            auto raw = transport.receive();
            if (!raw)
                return false;

            json msg = json::parse(*raw, nullptr, false);
            if (!msg.is_discarded())
            {
                auto method = msg.find("method");
                if (method != msg.end() && *method == "frame")
                    return true;
            }
        }
    }

    // A click (down then up) at a point as a window.inject events array.
    json clickEvents(float x, float y)
    {
        return json::array({
            { { "kind", "pointerButton" }, { "x", x }, { "y", y },
              { "button", 1 }, { "state", "down" } },
            { { "kind", "pointerButton" }, { "x", x }, { "y", y },
              { "button", 1 }, { "state", "up" } },
        });
    }
} // namespace

// --- Fake-window unit tests (no App) -------------------------------------

namespace
{
    // A test double recording what the session drives it with. Identity is its
    // id; a name is surfaced only inside introspection (never on the wire).
    class FakeAgentWindow : public AgentWindow
    {
    public:
        FakeAgentWindow(btl::UniqueId id, std::string name)
            : id_(id), name_(std::move(name)) {}

        btl::UniqueId id() const override { return id_; }

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

        // A synthetic render-tree snapshot with a recognizable root node and a
        // single text run, so a render reply can be asserted against without a
        // real render tree. The text is keyed to the window name so a
        // multi-window reply can be matched entry-by-entry.
        avg::Snapshot snapshot() const override
        {
            avg::SnapshotNode root;
            root.type = "FakeShape";
            root.obb = avg::Obb(avg::Vector2f(100.0f, 50.0f));
            root.text.push_back(avg::SnapshotText{
                    "fake:" + name_,
                    avg::Obb(avg::Vector2f(80.0f, 20.0f)) });

            avg::Snapshot snap;
            snap.time = std::chrono::milliseconds(0);
            snap.obb = avg::Obb(avg::Vector2f(200.0f, 100.0f));
            snap.root = std::move(root);
            return snap;
        }

        void advance(std::chrono::microseconds dt) override
        {
            ++advances_;
            totalDt_ += dt;
        }

        int buttonInjects() const { return buttonInjects_; }
        int advances() const { return advances_; }

    private:
        btl::UniqueId id_;
        std::string name_;
        int buttonInjects_ = 0;
        int advances_ = 0;
        std::chrono::microseconds totalDt_{0};
        std::string text_;
    };

    // An AgentApp over a fixed window set: the reconcile is a no-op (the fakes
    // never open or close), and the live set is always the same fakes.
    AgentApp staticApp(AgentWindows windows)
    {
        AgentApp app;
        app.reconcile = [](std::chrono::microseconds) {};
        app.liveWindows = [windows] { return windows; };
        return app;
    }

    // Run a session over `fakes` on a background thread against a loopback
    // endpoint, exposing the connected client side as a JSON-RPC channel.
    struct SessionFixture
    {
        std::unique_ptr<TransportListener> listener;
        std::unique_ptr<Transport> client;
        std::thread serverThread;
        int nextId = 1;

        SessionFixture(std::string const& name, AgentWindows windows)
        {
            auto endpoint = uniqueEndpoint(name);
            listener = listen(endpoint);

            serverThread = std::thread([this, windows]
                {
                    auto server = listener->accept();
                    runSession(staticApp(windows), *server);
                });

            client = connect(endpoint);
        }

        json call(std::string const& method, json params = json::object())
        {
            return rpc(*client, nextId++, method, std::move(params));
        }

        // A notification: no id, and so no reply is read.
        void notify(std::string const& method, json params = json::object())
        {
            json request = { { "jsonrpc", "2.0" }, { "method", method } };
            if (!params.empty())
                request["params"] = params;
            client->send(request.dump());
        }

        // Send a raw (possibly malformed) frame and read the one reply.
        json raw(std::string const& frame)
        {
            client->send(frame);
            auto reply = client->receive();
            return reply ? json::parse(*reply, nullptr, false) : json();
        }

        ~SessionFixture()
        {
            // Dropping the channel (no shutdown) must end runSession.
            client.reset();
            serverThread.join();
        }
    };
} // namespace

TEST(session, windowListReturnsIdsOnly)
{
    auto idA = btl::makeUniqueId();
    auto idB = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    FakeAgentWindow b(idB, "w1");
    SessionFixture s("list", { a, b });

    auto reply = s.call("window.list");
    auto const& windows = reply.at("result").at("windows");
    ASSERT_EQ(2u, windows.size());

    EXPECT_EQ(idA.getValue(), windows.at(0).at("id").get<uint64_t>());
    EXPECT_EQ(idB.getValue(), windows.at(1).at("id").get<uint64_t>());
    // Identity is id alone: no title on the wire.
    EXPECT_FALSE(windows.at(0).contains("title"));

    EXPECT_EQ(0, a.advances());
    EXPECT_EQ(0, b.advances());
}

TEST(session, introspectResolvesByIdAndErrorsOnUnknown)
{
    auto idA = btl::makeUniqueId();
    auto idB = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    FakeAgentWindow b(idB, "w1");
    SessionFixture s("introspect", { a, b });

    auto reply = s.call("window.introspect", { { "window", idB.getValue() } });
    EXPECT_EQ("w1", reply.at("result").at("introspection").at("name"));

    auto stray = btl::makeUniqueId();
    auto err = s.call("window.introspect", { { "window", stray.getValue() } });
    EXPECT_EQ(-32602, err.at("error").at("code").get<int>());
}

// window.renderTree resolves the window by id and returns its render-tree
// snapshot as a raw JSON sub-object (avg's schema), not a quoted string. The
// fake's synthetic snapshot carries a known node type and text run.
TEST(session, renderTreeResolvesByIdAndErrorsOnUnknown)
{
    auto idA = btl::makeUniqueId();
    auto idB = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    FakeAgentWindow b(idB, "w1");
    SessionFixture s("rendertree", { a, b });

    auto reply = s.call("window.renderTree", { { "window", idB.getValue() } });
    auto const& tree = reply.at("result").at("renderTree");

    // Embedded as an object, so it is navigable in place rather than a string.
    ASSERT_TRUE(tree.is_object());
    EXPECT_EQ(avg::Snapshot::version, tree.at("version").get<int>());

    auto const& root = tree.at("root");
    EXPECT_EQ("FakeShape", root.at("type").get<std::string>());
    ASSERT_EQ(1u, root.at("text").size());
    EXPECT_EQ("fake:w1", root.at("text").at(0).at("text").get<std::string>());

    // Observation only: neither window advances.
    EXPECT_EQ(0, a.advances());
    EXPECT_EQ(0, b.advances());

    // An unknown id is invalid params, like window.introspect.
    auto stray = btl::makeUniqueId();
    auto err = s.call("window.renderTree", { { "window", stray.getValue() } });
    EXPECT_EQ(-32602, err.at("error").at("code").get<int>());
}

TEST(session, injectRoutesByIdThenStepAdvancesAllWindows)
{
    auto idA = btl::makeUniqueId();
    auto idB = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    FakeAgentWindow b(idB, "w1");
    SessionFixture s("inject", { a, b });

    auto injectReply = s.call("window.inject", {
            { "window", idB.getValue() },
            { "events", json::array({
                { { "kind", "pointerButton" }, { "x", 1 }, { "y", 2 },
                  { "button", 1 }, { "state", "down" } } }) },
            });
    EXPECT_TRUE(injectReply.at("result").is_object());

    // The inject routed to b only; neither window has advanced yet.
    EXPECT_EQ(0, a.buttonInjects());
    EXPECT_EQ(1, b.buttonInjects());
    EXPECT_EQ(0, a.advances());
    EXPECT_EQ(0, b.advances());

    auto stepReply = s.call("app.step", { { "dt_us", 16667 } });
    EXPECT_EQ("paused", stepReply.at("result").at("state"));
    EXPECT_EQ(1u, stepReply.at("result").at("frame").get<uint64_t>());

    // The step advanced every live window once.
    EXPECT_EQ(1, a.advances());
    EXPECT_EQ(1, b.advances());
}

TEST(session, stepCountAdvancesAndFrameIsMonotonic)
{
    auto idA = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    SessionFixture s("step", { a });

    auto first = s.call("app.step", { { "count", 2 }, { "dt_us", 1000 } });
    EXPECT_EQ(2u, first.at("result").at("frame").get<uint64_t>());
    EXPECT_EQ(2, a.advances());

    auto second = s.call("app.step");
    EXPECT_EQ(3u, second.at("result").at("frame").get<uint64_t>());
    EXPECT_EQ(3, a.advances());
}

TEST(session, injectToUnknownWindowIsInvalidParams)
{
    auto idA = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    SessionFixture s("injectbad", { a });

    auto stray = btl::makeUniqueId();
    auto err = s.call("window.inject", {
            { "window", stray.getValue() },
            { "events", json::array() },
            });
    EXPECT_EQ(-32602, err.at("error").at("code").get<int>());
    EXPECT_EQ(0, a.buttonInjects());
}

TEST(session, injectRejectsOutOfRangeIndicesAtomically)
{
    auto idA = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    SessionFixture s("injectrange", { a });

    // A zero button would underflow the seam's button array; reject it and
    // apply none of the batch.
    auto badButton = s.call("window.inject", {
            { "window", idA.getValue() },
            { "events", json::array({
                { { "kind", "pointerButton" }, { "x", 1 }, { "y", 2 },
                  { "button", 0 }, { "state", "down" } } }) },
            });
    EXPECT_EQ(-32602, badButton.at("error").at("code").get<int>());
    EXPECT_EQ(0, a.buttonInjects());

    // An out-of-range pointer id is rejected the same way.
    auto badPointer = s.call("window.inject", {
            { "window", idA.getValue() },
            { "events", json::array({
                { { "kind", "pointerButton" }, { "x", 1 }, { "y", 2 },
                  { "pointer", 99 }, { "button", 1 }, { "state", "down" } } }) },
            });
    EXPECT_EQ(-32602, badPointer.at("error").at("code").get<int>());
    EXPECT_EQ(0, a.buttonInjects());

    // A valid batch after the rejections still applies normally.
    auto good = s.call("window.inject", {
            { "window", idA.getValue() },
            { "events", json::array({
                { { "kind", "pointerButton" }, { "x", 1 }, { "y", 2 },
                  { "button", 1 }, { "state", "down" } } }) },
            });
    EXPECT_TRUE(good.at("result").is_object());
    EXPECT_EQ(1, a.buttonInjects());
}

TEST(session, injectRejectsBatchAtomicallyBeforeApplying)
{
    auto idA = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    SessionFixture s("injectatomic", { a });

    // First event is valid, second is malformed: the whole batch must reject
    // without the first event reaching the seam.
    auto err = s.call("window.inject", {
            { "window", idA.getValue() },
            { "events", json::array({
                { { "kind", "pointerButton" }, { "x", 1 }, { "y", 2 },
                  { "button", 1 }, { "state", "down" } },
                { { "kind", "pointerButton" }, { "x", 1 }, { "y", 2 },
                  { "button", 0 }, { "state", "up" } } }) },
            });
    EXPECT_EQ(-32602, err.at("error").at("code").get<int>());
    EXPECT_EQ(0, a.buttonInjects());
}

TEST(session, describeReportsTheRegistry)
{
    FakeAgentWindow a(btl::makeUniqueId(), "w0");
    SessionFixture s("describe", { a });

    auto reply = s.call("system.describe");
    auto const& methods = reply.at("result").at("methods");
    ASSERT_TRUE(methods.is_array());

    std::vector<std::string> names;
    json introspectParams;
    for (auto const& m : methods)
    {
        names.push_back(m.at("name").get<std::string>());
        if (m.at("name") == "window.introspect")
            introspectParams = m.at("params");
    }

    auto has = [&](std::string const& n)
    {
        return std::find(names.begin(), names.end(), n) != names.end();
    };

    EXPECT_TRUE(has("system.describe"));
    EXPECT_TRUE(has("app.run"));
    EXPECT_TRUE(has("app.pause"));
    EXPECT_TRUE(has("app.step"));
    EXPECT_TRUE(has("app.shutdown"));
    EXPECT_TRUE(has("app.setFrameNotifications"));
    EXPECT_TRUE(has("window.list"));
    EXPECT_TRUE(has("window.introspect"));
    EXPECT_TRUE(has("window.inject"));
    // renderTree is served in this build (it rides on avg's snapshot).
    EXPECT_TRUE(has("window.renderTree"));

    // The parameter schema is first-class: window.introspect requires `window`.
    ASSERT_TRUE(introspectParams.is_array());
    ASSERT_EQ(1u, introspectParams.size());
    EXPECT_EQ("window", introspectParams.at(0).at("name"));
    EXPECT_EQ(true, introspectParams.at(0).at("required"));
}

// --- Flow control: run / pause / step / notifications / shutdown ----------

TEST(session, runThenPauseReportsAdvancedFrame)
{
    auto idA = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    SessionFixture s("runpause", { a });

    // Turn on frame notifications so the client can wait for a concrete frame
    // rather than racing the free run with a sleep.
    int nId = s.nextId++;
    sendRequest(*s.client, nId, "app.setFrameNotifications",
            { { "enabled", true } });
    awaitResponse(*s.client, nId);

    int runId = s.nextId++;
    sendRequest(*s.client, runId, "app.run");
    EXPECT_EQ("running",
            awaitResponse(*s.client, runId).at("result").at("state"));

    // At least one frame ran while free-running.
    ASSERT_TRUE(awaitFrameNotification(*s.client));

    int pauseId = s.nextId++;
    sendRequest(*s.client, pauseId, "app.pause");
    auto pauseReply = awaitResponse(*s.client, pauseId);
    EXPECT_EQ("paused", pauseReply.at("result").at("state"));
    EXPECT_GE(pauseReply.at("result").at("frame").get<uint64_t>(), 1u);
    EXPECT_GE(a.advances(), 1);
}

TEST(session, pausedFrameIsStableWithoutAStep)
{
    auto idA = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    SessionFixture s("stable", { a });

    // Reach a known frame from paused; no notifications, so nothing interleaves.
    int stepId = s.nextId++;
    sendRequest(*s.client, stepId, "app.step", { { "count", 3 } });
    uint64_t frame =
        awaitResponse(*s.client, stepId).at("result").at("frame").get<uint64_t>();
    EXPECT_EQ(3u, frame);
    EXPECT_EQ(3, a.advances());

    // A pause (a query at a frame boundary) shows the same frame — paused holds.
    int pauseId = s.nextId++;
    sendRequest(*s.client, pauseId, "app.pause");
    EXPECT_EQ(frame,
            awaitResponse(*s.client, pauseId).at("result").at("frame")
                .get<uint64_t>());

    // And a window.list issued while paused advances nothing further.
    int listId = s.nextId++;
    sendRequest(*s.client, listId, "window.list");
    awaitResponse(*s.client, listId);
    EXPECT_EQ(3, a.advances());
}

TEST(session, stepFromPausedAdvancesExactlyAndStaysPaused)
{
    auto idA = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    SessionFixture s("steppaused", { a });

    int firstId = s.nextId++;
    sendRequest(*s.client, firstId, "app.step", { { "count", 4 } });
    auto first = awaitResponse(*s.client, firstId);
    EXPECT_EQ("paused", first.at("result").at("state"));
    EXPECT_EQ(4u, first.at("result").at("frame").get<uint64_t>());
    EXPECT_EQ(4, a.advances());

    // Still paused: a following default step continues from 4, one at a time.
    int secondId = s.nextId++;
    sendRequest(*s.client, secondId, "app.step");
    auto second = awaitResponse(*s.client, secondId);
    EXPECT_EQ(5u, second.at("result").at("frame").get<uint64_t>());
    EXPECT_EQ(5, a.advances());
}

TEST(session, queryWhileRunningIsCoherentAndKeepsRunning)
{
    auto idA = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    SessionFixture s("querying", { a });

    int nId = s.nextId++;
    sendRequest(*s.client, nId, "app.setFrameNotifications",
            { { "enabled", true } });
    awaitResponse(*s.client, nId);

    int runId = s.nextId++;
    sendRequest(*s.client, runId, "app.run");
    awaitResponse(*s.client, runId);
    ASSERT_TRUE(awaitFrameNotification(*s.client));

    // A query mid-run returns coherent data (the one live window)...
    int listId = s.nextId++;
    sendRequest(*s.client, listId, "window.list");
    auto list = awaitResponse(*s.client, listId);
    ASSERT_EQ(1u, list.at("result").at("windows").size());
    EXPECT_EQ(idA.getValue(),
            list.at("result").at("windows").at(0).at("id").get<uint64_t>());

    // ...and the app is still running afterward (more frames arrive).
    ASSERT_TRUE(awaitFrameNotification(*s.client));
}

TEST(session, frameNotificationsOffIsSilent)
{
    auto idA = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    SessionFixture s("silent", { a });

    // Default is off: run, then pause, and no frame notification interleaves.
    int runId = s.nextId++;
    sendRequest(*s.client, runId, "app.run");
    awaitResponse(*s.client, runId);

    int pauseId = s.nextId++;
    sendRequest(*s.client, pauseId, "app.pause");
    int notifs = 0;
    awaitResponse(*s.client, pauseId, &notifs);
    EXPECT_EQ(0, notifs);
}

TEST(session, frameNotificationsOnEmitFrames)
{
    auto idA = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    SessionFixture s("emitting", { a });

    int nId = s.nextId++;
    sendRequest(*s.client, nId, "app.setFrameNotifications",
            { { "enabled", true } });
    awaitResponse(*s.client, nId);

    int runId = s.nextId++;
    sendRequest(*s.client, runId, "app.run");
    awaitResponse(*s.client, runId);

    EXPECT_TRUE(awaitFrameNotification(*s.client));
}

TEST(session, cleanShutdownFromRunningTerminates)
{
    auto idA = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");

    auto endpoint = uniqueEndpoint("runshutdown");
    auto listener = listen(endpoint);

    std::atomic<bool> returned{ false };
    std::thread server([&]
        {
            auto conn = listener->accept();
            runSession(staticApp({ a }), *conn);
            returned.store(true);
        });

    auto client = connect(endpoint);
    int id = 1;

    sendRequest(*client, id, "app.run");
    EXPECT_EQ("running",
            awaitResponse(*client, id++).at("result").at("state"));

    // Shutdown while the reader is blocked in receive(): the app thread must
    // wake it (close the transport) and join it, then return from runSession.
    sendRequest(*client, id, "app.shutdown");
    EXPECT_TRUE(awaitResponse(*client, id++).at("result").is_object());

    // Bound the wait — a correct shutdown returns promptly. Poll rather than
    // block, so a regression fails loudly instead of hanging the whole suite.
    for (int i = 0; i < 500 && !returned.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(returned.load());

    if (returned.load())
        server.join();
    else
        server.detach(); // Already failed; do not hang the test binary.

    client.reset();
}

// --- Envelope unit tests --------------------------------------------------

TEST(session, unknownMethodIsMethodNotFound)
{
    FakeAgentWindow a(btl::makeUniqueId(), "w0");
    SessionFixture s("unknown", { a });

    auto reply = s.call("frobnicate");
    EXPECT_EQ(-32601, reply.at("error").at("code").get<int>());
}

TEST(session, malformedJsonIsParseError)
{
    FakeAgentWindow a(btl::makeUniqueId(), "w0");
    SessionFixture s("malformed", { a });

    auto reply = s.raw("not json");
    EXPECT_EQ(-32700, reply.at("error").at("code").get<int>());
    EXPECT_TRUE(reply.at("id").is_null());
}

TEST(session, missingRequiredParamIsInvalidParams)
{
    FakeAgentWindow a(btl::makeUniqueId(), "w0");
    SessionFixture s("missingparam", { a });

    // window.introspect requires `window`.
    auto reply = s.call("window.introspect");
    EXPECT_EQ(-32602, reply.at("error").at("code").get<int>());
}

TEST(session, notificationGetsNoReply)
{
    auto idA = btl::makeUniqueId();
    FakeAgentWindow a(idA, "w0");
    SessionFixture s("notify", { a });

    // A notification (no id) is handled but never answered. If it had drawn a
    // reply, the next receive would read it instead of window.list's response.
    s.notify("app.step", { { "count", 1 } });

    auto reply = s.call("window.list");
    EXPECT_EQ(s.nextId - 1, reply.at("id").get<int>());
    EXPECT_TRUE(reply.contains("result"));

    // The notification did run: the fake advanced once.
    EXPECT_EQ(1, a.advances());
}

// --- Real-App integration: describe / list / introspect -------------------

namespace
{
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

    std::optional<double> findCount(json const& node)
    {
        auto name = node.find("name");
        if (name != node.end() && *name == "counter")
        {
            auto data = node.find("data");
            if (data != node.end())
            {
                auto count = data->find("count");
                if (count != data->end() && count->is_number())
                    return count->get<double>();
            }
        }

        auto children = node.find("children");
        if (children != node.end() && children->is_array())
            for (auto const& child : *children)
                if (auto found = findCount(child))
                    return found;

        return std::nullopt;
    }

    // Whether an introspection subtree carries a node with the given name.
    bool treeHasName(json const& node, std::string const& name)
    {
        auto n = node.find("name");
        if (n != node.end() && *n == name)
            return true;

        auto children = node.find("children");
        if (children != node.end() && children->is_array())
            for (auto const& child : *children)
                if (treeHasName(child, name))
                    return true;

        return false;
    }
} // namespace

TEST(session, describeListIntrospectDriveARealApp)
{
    auto endpoint = uniqueEndpoint("agentcore");
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
    int id = 1;

    // system.describe: the registry lists the core methods.
    auto describe = rpc(*agent, id++, "system.describe");
    bool hasStep = false;
    for (auto const& m : describe.at("result").at("methods"))
        if (m.at("name") == "app.step")
            hasStep = true;
    EXPECT_TRUE(hasStep);

    // window.list: exactly one window; capture its id.
    auto list = rpc(*agent, id++, "window.list");
    ASSERT_EQ(1u, list.at("result").at("windows").size());
    uint64_t windowId =
        list.at("result").at("windows").at(0).at("id").get<uint64_t>();

    // window.introspect: the counter starts at 0.
    auto intro0 = rpc(*agent, id++, "window.introspect",
            { { "window", windowId } });
    auto count0 = findCount(intro0.at("result").at("introspection"));
    ASSERT_TRUE(count0.has_value());
    EXPECT_EQ(0.0, *count0);

    // Act by id: queue a click, step one frame, then observe the bump.
    rpc(*agent, id++, "window.inject",
            { { "window", windowId }, { "events", clickEvents(400.0f, 300.0f) } });
    auto step = rpc(*agent, id++, "app.step", { { "dt_us", 16667 } });
    EXPECT_EQ("paused", step.at("result").at("state"));

    auto intro1 = rpc(*agent, id++, "window.introspect",
            { { "window", windowId } });
    auto count1 = findCount(intro1.at("result").at("introspection"));
    ASSERT_TRUE(count1.has_value());
    EXPECT_EQ(1.0, *count1);

    rpc(*agent, id++, "app.shutdown");
    appThread.join();
}

// --- Render integration: a real render tree over the wire ----------------

namespace
{
    // Whether a rendertree node (or any descendant) draws the given text run.
    bool treeHasText(json const& node, std::string const& text)
    {
        auto runs = node.find("text");
        if (runs != node.end() && runs->is_array())
            for (auto const& run : *runs)
            {
                auto t = run.find("text");
                if (t != run.end() && t->is_string() && *t == text)
                    return true;
            }

        auto children = node.find("children");
        if (children != node.end() && children->is_array())
            for (auto const& child : *children)
                if (treeHasText(child, text))
                    return true;

        return false;
    }

    // Whether a rendertree node (or any descendant) has the given node type.
    bool treeHasType(json const& node, std::string const& type)
    {
        auto t = node.find("type");
        if (t != node.end() && t->is_string() && *t == type)
            return true;

        auto children = node.find("children");
        if (children != node.end() && children->is_array())
            for (auto const& child : *children)
                if (treeHasType(child, type))
                    return true;

        return false;
    }
} // namespace

// A real headless app whose one window draws a "hello" label: window.renderTree
// returns that window's render-tree snapshot as a schema-version-1 document
// embedded as a raw object, and the drawn text is recoverable from it.
TEST(session, renderTreeReturnsARealWindowsRenderTree)
{
    auto endpoint = uniqueEndpoint("agentrender");
    auto listener = listen(endpoint);

    std::thread appThread([&]
    {
        App()
            .platform(ase::makeDummyPlatform())
            .agentic(true)
            .agentEndpoint(endpoint)
            .addWindow(
                    window(bq::signal::constant<std::string>("Render"),
                        label("hello")))
            .run();
    });

    auto agent = listener->accept();
    int id = 1;

    // Build one frame so the render tree exists (agentic mode never free-runs).
    auto step = rpc(*agent, id++, "app.step", { { "dt_us", 16667 } });
    EXPECT_EQ("paused", step.at("result").at("state"));

    // Address the one window by id.
    auto list = rpc(*agent, id++, "window.list");
    ASSERT_EQ(1u, list.at("result").at("windows").size());
    uint64_t windowId =
        list.at("result").at("windows").at(0).at("id").get<uint64_t>();

    // Observe the render tree: a schema-version-1 object, not a quoted string.
    auto reply = rpc(*agent, id++, "window.renderTree",
            { { "window", windowId } });
    auto const& tree = reply.at("result").at("renderTree");
    ASSERT_TRUE(tree.is_object());
    EXPECT_EQ(avg::Snapshot::version, tree.at("version").get<int>());

    auto const& root = tree.at("root");
    ASSERT_TRUE(root.is_object());

    // The label is a shape leaf that draws the text "hello".
    EXPECT_TRUE(treeHasType(root, "ShapeNode"));
    EXPECT_TRUE(treeHasText(root, "hello"));

    rpc(*agent, id++, "app.shutdown");
    appThread.join();
}

// --- Dynamic-windows integration: the window set is live, keyed by id -----

TEST(session, dynamicWindowsOpenAndCloseById)
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
    int id = 1;

    // Observe: exactly one window, "main".
    auto list0 = rpc(*agent, id++, "window.list");
    ASSERT_EQ(1u, list0.at("result").at("windows").size());
    uint64_t mainId =
        list0.at("result").at("windows").at(0).at("id").get<uint64_t>();

    auto mainTree = rpc(*agent, id++, "window.introspect",
            { { "window", mainId } });
    EXPECT_TRUE(treeHasName(mainTree.at("result").at("introspection"),
                "mainRoot"));

    // Act: click main's centre, then step. The handler opens the child, and the
    // next list shows both windows.
    rpc(*agent, id++, "window.inject",
            { { "window", mainId }, { "events", clickEvents(400.0f, 300.0f) } });
    rpc(*agent, id++, "app.step", { { "dt_us", 16667 } });

    auto list1 = rpc(*agent, id++, "window.list");
    ASSERT_EQ(2u, list1.at("result").at("windows").size());

    // Identify the child: the id that is not main's.
    uint64_t childWireId = 0;
    for (auto const& w : list1.at("result").at("windows"))
    {
        uint64_t wid = w.at("id").get<uint64_t>();
        if (wid != mainId)
            childWireId = wid;
    }
    ASSERT_NE(0u, childWireId);

    auto childTree = rpc(*agent, id++, "window.introspect",
            { { "window", childWireId } });
    EXPECT_TRUE(treeHasName(childTree.at("result").at("introspection"),
                "childRoot"));

    // Act: click the child's centre, then step. Its handler closes it, and the
    // set is back to just "main".
    rpc(*agent, id++, "window.inject",
            { { "window", childWireId },
              { "events", clickEvents(400.0f, 300.0f) } });
    rpc(*agent, id++, "app.step", { { "dt_us", 16667 } });

    auto list2 = rpc(*agent, id++, "window.list");
    ASSERT_EQ(1u, list2.at("result").at("windows").size());
    EXPECT_EQ(mainId,
            list2.at("result").at("windows").at(0).at("id").get<uint64_t>());

    rpc(*agent, id++, "app.shutdown");
    appThread.join();
}
