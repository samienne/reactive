#include "remote/base64.h"
#include "remote/blobstore.h"
#include "remote/capture.h"
#include "remote/dispatcher.h"
#include "remote/introspectionjson.h"
#include "remote/png.h"
#include "remote/screenshot.h"
#include "remote/streamtransport.h"
#include "remote/tcptransport.h"
#include "scriptedtransport.h"

#include <bqui/remote/remotedriver.h>
#include <bqui/remote/transport.h>

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
#include <ase/dummyrendercontext.h>
#include <ase/rendercontext.h>
#include <ase/window.h>
#include <avg/color.h>
#include <avg/obb.h>
#include <avg/rect.h>

#include <btl/uniqueid.h>
#include <btl/runloop.h>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#   include <process.h>
#else
#   include <unistd.h>
#endif

using namespace bqui;
using namespace bqui::widget;
using namespace bqui::remote;

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
    // A test double recording what the driver injects into it. Identity is its
    // id; a name is surfaced only inside introspection (never on the wire).
    // Frame advancing is app-level now, so there is no per-window advance here.
    class FakeRemoteWindow : public RemoteWindow
    {
    public:
        FakeRemoteWindow(btl::UniqueId id, std::string name)
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

        avg::Snapshot snapshot() const override
        {
            avg::Snapshot snap;
            snap.obb = avg::Obb(ase::Vector2f(800.0f, 600.0f));
            avg::SnapshotNode root;
            root.type = "Fake";
            root.obb = snap.obb;
            root.text.push_back({ name_, snap.obb });
            snap.root = std::move(root);
            return snap;
        }

        int buttonInjects() const { return buttonInjects_; }

    private:
        btl::UniqueId id_;
        std::string name_;
        int buttonInjects_ = 0;
        std::string text_;
    };

    // A fake that can capture: it holds each armed callback until the test
    // completes it, as a render thread would, unless told to serve a result
    // at once. It presents whenever the fixture steps, and a widget layout set
    // for the next present takes effect there.
    class CapturableFakeWindow : public FakeRemoteWindow
    {
    public:
        using FakeRemoteWindow::FakeRemoteWindow;

        using Widget = std::optional<std::pair<avg::UniqueId, LogicalRect>>;

        Introspection introspect() const override
        {
            Introspection root = FakeRemoteWindow::introspect();

            std::lock_guard<std::mutex> lock(widgetMutex_);
            if (widget_)
            {
                LogicalRect const& r = widget_->second;
                Introspection child;
                child.id = widget_->first;
                child.obb = avg::Obb(avg::Rect(ase::Vector2f(r.x, r.y),
                            ase::Vector2f(r.width, r.height)));
                root.children.push_back(
                        makeIntrospectionChild(std::move(child)));
            }

            return root;
        }

        bool canCapture() const override { return true; }

        void captureNextFrame(ase::CaptureCallback callback) override
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (serveWith_)
            {
                auto result = *serveWith_;
                lock.unlock();
                callback(std::move(result));
                return;
            }

            armed_.push_back(std::move(callback));
            armedChanged_.notify_all();
        }

        void requestFrame() override { ++frameRequests_; }

        float scalingFactor() const override { return scalingFactor_; }

        std::uint64_t presentCount() const override { return presents_; }

        void present()
        {
            std::lock_guard<std::mutex> lock(widgetMutex_);
            if (nextWidget_)
            {
                widget_ = *nextWidget_;
                nextWidget_.reset();
            }
            ++presents_;
        }

        void setWidget(Widget widget)
        {
            std::lock_guard<std::mutex> lock(widgetMutex_);
            widget_ = std::move(widget);
        }

        void setWidgetAtNextPresent(Widget widget)
        {
            std::lock_guard<std::mutex> lock(widgetMutex_);
            nextWidget_ = std::move(widget);
        }

        std::vector<ase::CaptureCallback> takeArmed()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return std::move(armed_);
        }

        // Waits until `count` captures are armed, then takes them.
        std::vector<ase::CaptureCallback> awaitArmed(size_t count)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            armedChanged_.wait_for(lock, std::chrono::seconds(10),
                    [&] { return armed_.size() >= count; });
            return std::move(armed_);
        }

        void serveWith(ase::CaptureResult result)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            serveWith_ = std::move(result);
        }

        void setScalingFactor(float factor) { scalingFactor_ = factor; }

        int frameRequests() const { return frameRequests_.load(); }

    private:
        std::mutex mutex_;
        std::condition_variable armedChanged_;
        std::vector<ase::CaptureCallback> armed_;
        std::optional<ase::CaptureResult> serveWith_;
        std::atomic<int> frameRequests_{ 0 };
        float scalingFactor_ = 1.0f;
        mutable std::mutex widgetMutex_;
        Widget widget_;
        std::optional<Widget> nextWidget_;
        std::atomic<std::uint64_t> presents_{ 0 };
    };

    // Serve the driver over `fakes` on a background thread against a loopback
    // endpoint, exposing the connected client side as a JSON-RPC channel. The
    // driver runs on a plain loop with no pause token (there is no real platform
    // to pause): its own run/pause/step flow control drives the fake app frame.
    struct SessionFixture
    {
        std::unique_ptr<TransportListener> listener;
        std::unique_ptr<Transport> client;
        std::thread serverThread;
        int nextId = 1;

        // Frames the driver drove through the seam. It tracks the driver's own
        // frame counter (both bump together), so it is the observable proof a
        // step or a free-running tick happened.
        std::atomic<int> steps{ 0 };
        std::mutex stepsMutex;
        std::condition_variable stepsChanged;

        SessionFixture(std::string const& name, RemoteWindows windows)
        {
            auto endpoint = uniqueEndpoint(name);
            listener = listen(endpoint);
            start(endpoint, std::move(windows));
        }

        SessionFixture(std::string const& name, RemoteWindows windows,
                std::chrono::milliseconds stepTimeout, BlobLimits blobLimits)
        {
            auto endpoint = uniqueEndpoint(name);
            listener = listen(endpoint);
            start(endpoint, std::move(windows), stepTimeout, blobLimits);
        }

        struct Tcp {};

        SessionFixture(Tcp, RemoteWindows windows)
        {
            auto tcp = tcpListen(*parseTcpEndpoint("127.0.0.1:0"));
            auto endpoint = "127.0.0.1:" + std::to_string(tcp->port());
            listener = std::move(tcp);
            start(endpoint, std::move(windows));
        }

        void start(std::string const& endpoint, RemoteWindows windows,
                std::chrono::milliseconds stepTimeout = kStepTimeout,
                BlobLimits blobLimits = {})
        {
            serverThread = std::thread([this, windows, stepTimeout,
                    blobLimits]
                {
                    auto server = listener->accept();

                    btl::RunLoop loop;

                    RemoteApp app;
                    app.step = [this, windows](std::chrono::microseconds)
                    {
                        for (auto& window : windows)
                        {
                            if (auto* capturable =
                                    dynamic_cast<CapturableFakeWindow*>(
                                        &window.get()))
                            {
                                capturable->present();
                            }
                        }

                        {
                            std::lock_guard<std::mutex> lock(stepsMutex);
                            ++steps;
                        }
                        stepsChanged.notify_all();
                        return true;
                    };
                    app.sync = [] {};
                    app.liveWindows = [windows] { return windows; };

                    RemoteDriver driver(loop, *server, std::move(app),
                            std::nullopt, stepTimeout, blobLimits);

                    loop.run();
                });

            client = connect(endpoint);
        }

        // Wait for the driver to have stepped `count` frames, or give up.
        bool awaitSteps(int count)
        {
            std::unique_lock<std::mutex> lock(stepsMutex);
            return stepsChanged.wait_for(lock, std::chrono::seconds(10),
                    [&] { return steps.load() >= count; });
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
            // Dropping the channel (no shutdown) must stop the driver's loop.
            client.reset();
            serverThread.join();
        }
    };

    [[nodiscard]] btl::RunLoop::Timer failUnlessStoppedSoon(
            btl::RunLoop::Controller& controller)
    {
        return controller.addTimer(std::chrono::seconds(10),
                [](btl::RunLoop::Controller& c)
                {
                    ADD_FAILURE() << "the capture result never arrived";
                    c.stop();
                });
    }
} // namespace

TEST(session, windowListIdentifiesById)
{
    auto idA = btl::makeUniqueId();
    auto idB = btl::makeUniqueId();
    FakeRemoteWindow a(idA, "w0");
    FakeRemoteWindow b(idB, "w1");
    SessionFixture s("list", { a, b });

    auto reply = s.call("window.list");
    auto const& windows = reply.at("result").at("windows");
    ASSERT_EQ(2u, windows.size());

    EXPECT_EQ(idA.getValue(), windows.at(0).at("id").get<uint64_t>());
    EXPECT_EQ(idB.getValue(), windows.at(1).at("id").get<uint64_t>());
    // Identity is id alone: no title on the wire.
    EXPECT_FALSE(windows.at(0).contains("title"));

    // A pure query advances no frame.
    EXPECT_EQ(0, s.steps.load());
}

TEST(session, windowListReportsCanCapturePerWindow)
{
    FakeRemoteWindow plain(btl::makeUniqueId(), "plain");
    CapturableFakeWindow capturable(btl::makeUniqueId(), "capturable");
    SessionFixture s("cancapture", { plain, capturable });

    auto reply = s.call("window.list");
    auto const& windows = reply.at("result").at("windows");
    ASSERT_EQ(2u, windows.size());

    EXPECT_FALSE(windows.at(0).at("canCapture").get<bool>());
    EXPECT_TRUE(windows.at(1).at("canCapture").get<bool>());
}

// --- Capture delivery: results reach the loop thread ----------------------

TEST(session, captureResultReachesTheLoopThread)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "w");
    btl::RunLoop loop;
    auto alive = std::make_shared<bool>(true);

    std::optional<ase::CaptureResult> received;
    std::thread::id receivedOn;
    btl::RunLoop::Timer guard;

    loop.post([&](btl::RunLoop::Controller& controller)
        {
            guard = failUnlessStoppedSoon(controller);
            captureOnLoop(window, loop, alive,
                    [&](btl::RunLoop::Controller& c, ase::CaptureResult result)
                    {
                        received = std::move(result);
                        receivedOn = std::this_thread::get_id();
                        c.stop();
                    });

            auto armed = window.takeArmed();
            if (armed.size() != 1)
            {
                ADD_FAILURE() << "armed " << armed.size() << " captures";
                controller.stop();
                return;
            }

            std::thread([callback = std::move(armed.front())]
                {
                    ase::FrameCapture capture;
                    capture.presentIndex = 3;
                    capture.image.size = ase::Vector2i(1, 1);
                    capture.image.pixels = { 1, 2, 3, 4 };
                    callback(std::move(capture));
                }).join();
        });

    loop.run();

    EXPECT_EQ(std::this_thread::get_id(), receivedOn);
    ASSERT_TRUE(received && received->valid());
    EXPECT_EQ(3u, received->value().presentIndex);
    EXPECT_EQ((std::vector<std::uint8_t>{ 1, 2, 3, 4 }),
            received->value().image.pixels);
}

TEST(session, unsupportedCaptureIsDeliveredLaterNotInline)
{
    FakeRemoteWindow window(btl::makeUniqueId(), "w");
    btl::RunLoop loop;
    auto alive = std::make_shared<bool>(true);

    std::optional<ase::CaptureResult> received;
    bool armedReturned = false;
    btl::RunLoop::Timer guard;

    loop.post([&](btl::RunLoop::Controller& controller)
        {
            guard = failUnlessStoppedSoon(controller);
            captureOnLoop(window, loop, alive,
                    [&](btl::RunLoop::Controller& controller,
                        ase::CaptureResult result)
                    {
                        EXPECT_TRUE(armedReturned);
                        received = std::move(result);
                        controller.stop();
                    });
            armedReturned = true;
        });

    loop.run();

    ASSERT_TRUE(received && !received->valid());
    EXPECT_EQ(ase::ReadbackError::Unsupported, received->err());
}

TEST(session, captureResultIsDroppedOnceTheOwnerIsGone)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "w");
    btl::RunLoop loop;
    auto alive = std::make_shared<bool>(true);

    bool delivered = false;

    loop.post([&](btl::RunLoop::Controller& controller)
        {
            captureOnLoop(window, loop, alive,
                    [&](btl::RunLoop::Controller&, ase::CaptureResult)
                    {
                        delivered = true;
                    });

            alive.reset();
            auto armed = window.takeArmed();
            EXPECT_EQ(1u, armed.size());
            for (auto& callback : armed)
                callback(ase::ReadbackError::Cancelled);

            // Posted after the result, so it runs after the dropped delivery.
            controller.post([](btl::RunLoop::Controller& c) { c.stop(); });
        });

    loop.run();

    EXPECT_FALSE(delivered);
}

TEST(session, captureCompletionIsDestroyedOnTheLoopThread)
{
    struct RecordsDestroyingThread
    {
        explicit RecordsDestroyingThread(std::thread::id* where)
            : destroyedOn(where)
        {
        }

        ~RecordsDestroyingThread()
        {
            *destroyedOn = std::this_thread::get_id();
        }

        std::thread::id* destroyedOn;
    };

    CapturableFakeWindow window(btl::makeUniqueId(), "w");
    btl::RunLoop loop;
    auto alive = std::make_shared<bool>(true);

    std::thread::id destroyedOn;
    std::promise<void> loopDone;
    std::thread completer;
    btl::RunLoop::Timer guard;

    loop.post([&](btl::RunLoop::Controller& controller)
        {
            guard = failUnlessStoppedSoon(controller);
            auto recorder =
                std::make_shared<RecordsDestroyingThread>(&destroyedOn);
            captureOnLoop(window, loop, alive,
                    [recorder = std::move(recorder)](
                        btl::RunLoop::Controller& c, ase::CaptureResult)
                    {
                        c.stop();
                    });

            auto armed = window.takeArmed();
            ASSERT_EQ(1u, armed.size());

            // Holds the completed callback until the loop is done, so any copy
            // of the completion left in it would be destroyed last, here.
            completer = std::thread(
                    [callback = std::move(armed.front()),
                    done = loopDone.get_future()]() mutable
                    {
                        callback(ase::ReadbackError::Cancelled);
                        done.wait();
                    });
        });

    loop.run();
    loopDone.set_value();
    if (completer.joinable())
        completer.join();

    EXPECT_EQ(std::this_thread::get_id(), destroyedOn);
}

TEST(session, introspectResolvesByIdAndErrorsOnUnknown)
{
    auto idA = btl::makeUniqueId();
    auto idB = btl::makeUniqueId();
    FakeRemoteWindow a(idA, "w0");
    FakeRemoteWindow b(idB, "w1");
    SessionFixture s("introspect", { a, b });

    auto reply = s.call("window.introspect", { { "window", idB.getValue() } });
    EXPECT_EQ("w1", reply.at("result").at("introspection").at("name"));

    auto stray = btl::makeUniqueId();
    auto err = s.call("window.introspect", { { "window", stray.getValue() } });
    EXPECT_EQ(-32602, err.at("error").at("code").get<int>());
}

TEST(session, renderTreeResolvesByIdAndEmbedsTheSnapshot)
{
    auto idA = btl::makeUniqueId();
    auto idB = btl::makeUniqueId();
    FakeRemoteWindow a(idA, "w0");
    FakeRemoteWindow b(idB, "w1");
    SessionFixture s("renderTree", { a, b });

    auto reply = s.call("window.renderTree", { { "window", idB.getValue() } });
    // Embedded as a value, never a quoted string.
    auto const& tree = reply.at("result").at("renderTree");
    ASSERT_TRUE(tree.is_object());
    EXPECT_EQ("Fake", tree.at("root").at("type"));
    EXPECT_EQ("w1", tree.at("root").at("text").at(0).at("text"));

    auto stray = btl::makeUniqueId();
    auto err = s.call("window.renderTree", { { "window", stray.getValue() } });
    EXPECT_EQ(-32602, err.at("error").at("code").get<int>());
}

TEST(session, injectRoutesByIdThenStepAdvancesTheApp)
{
    auto idA = btl::makeUniqueId();
    auto idB = btl::makeUniqueId();
    FakeRemoteWindow a(idA, "w0");
    FakeRemoteWindow b(idB, "w1");
    SessionFixture s("inject", { a, b });

    auto injectReply = s.call("window.inject", {
            { "window", idB.getValue() },
            { "events", json::array({
                { { "kind", "pointerButton" }, { "x", 1 }, { "y", 2 },
                  { "button", 1 }, { "state", "down" } } }) },
            });
    EXPECT_TRUE(injectReply.at("result").is_object());

    // The inject routed to b only; no frame has advanced yet.
    EXPECT_EQ(0, a.buttonInjects());
    EXPECT_EQ(1, b.buttonInjects());
    EXPECT_EQ(0, s.steps.load());

    auto stepReply = s.call("app.step", { { "dt_us", 16667 } });
    EXPECT_EQ("paused", stepReply.at("result").at("state"));
    EXPECT_EQ(1u, stepReply.at("result").at("frame").get<uint64_t>());

    // The step advanced exactly one app frame.
    EXPECT_EQ(1, s.steps.load());
}

TEST(session, stepCountAdvancesAndFrameIsMonotonic)
{
    auto idA = btl::makeUniqueId();
    FakeRemoteWindow a(idA, "w0");
    SessionFixture s("step", { a });

    auto first = s.call("app.step", { { "count", 2 }, { "dt_us", 1000 } });
    EXPECT_EQ(2u, first.at("result").at("frame").get<uint64_t>());

    auto second = s.call("app.step");
    EXPECT_EQ(3u, second.at("result").at("frame").get<uint64_t>());
    EXPECT_EQ(3, s.steps.load());
}

TEST(session, injectToUnknownWindowIsInvalidParams)
{
    auto idA = btl::makeUniqueId();
    FakeRemoteWindow a(idA, "w0");
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
    FakeRemoteWindow a(idA, "w0");
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
    FakeRemoteWindow a(idA, "w0");
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

TEST(session, injectRejectsUnknownOrMissingKind)
{
    auto idA = btl::makeUniqueId();
    FakeRemoteWindow a(idA, "w0");
    SessionFixture s("injectkind", { a });

    // A mistyped shape (here the field is 'type', not 'kind', so 'kind' reads
    // empty) must be a loud error, not a silently dropped no-op.
    auto mistyped = s.call("window.inject", {
            { "window", idA.getValue() },
            { "events", json::array({
                { { "type", "pointerButton" }, { "x", 1 }, { "y", 2 },
                  { "button", 1 }, { "state", "down" } } }) },
            });
    EXPECT_EQ(-32602, mistyped.at("error").at("code").get<int>());
    EXPECT_EQ(0, a.buttonInjects());

    // An unrecognised kind is rejected the same way.
    auto unknown = s.call("window.inject", {
            { "window", idA.getValue() },
            { "events", json::array({
                { { "kind", "wiggle" }, { "x", 1 }, { "y", 2 } } }) },
            });
    EXPECT_EQ(-32602, unknown.at("error").at("code").get<int>());
    EXPECT_EQ(0, a.buttonInjects());
}

TEST(session, describeReportsTheRegistry)
{
    FakeRemoteWindow a(btl::makeUniqueId(), "w0");
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
    FakeRemoteWindow a(idA, "w0");
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
    EXPECT_GE(s.steps.load(), 1);
}

TEST(session, pausedFrameIsStableWithoutAStep)
{
    auto idA = btl::makeUniqueId();
    FakeRemoteWindow a(idA, "w0");
    SessionFixture s("stable", { a });

    // Reach a known frame from paused; no notifications, so nothing interleaves.
    int stepId = s.nextId++;
    sendRequest(*s.client, stepId, "app.step", { { "count", 3 } });
    uint64_t frame =
        awaitResponse(*s.client, stepId).at("result").at("frame").get<uint64_t>();
    EXPECT_EQ(3u, frame);

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
    EXPECT_EQ(3, s.steps.load());
}

TEST(session, stepFromPausedAdvancesExactlyAndStaysPaused)
{
    auto idA = btl::makeUniqueId();
    FakeRemoteWindow a(idA, "w0");
    SessionFixture s("steppaused", { a });

    int firstId = s.nextId++;
    sendRequest(*s.client, firstId, "app.step", { { "count", 4 } });
    auto first = awaitResponse(*s.client, firstId);
    EXPECT_EQ("paused", first.at("result").at("state"));
    EXPECT_EQ(4u, first.at("result").at("frame").get<uint64_t>());

    // Still paused: a following default step continues from 4, one at a time.
    int secondId = s.nextId++;
    sendRequest(*s.client, secondId, "app.step");
    auto second = awaitResponse(*s.client, secondId);
    EXPECT_EQ(5u, second.at("result").at("frame").get<uint64_t>());
    EXPECT_EQ(5, s.steps.load());
}

TEST(session, queryWhileRunningIsCoherentAndKeepsRunning)
{
    auto idA = btl::makeUniqueId();
    FakeRemoteWindow a(idA, "w0");
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
    FakeRemoteWindow a(idA, "w0");
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
    FakeRemoteWindow a(idA, "w0");
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
    FakeRemoteWindow a(idA, "w0");

    auto endpoint = uniqueEndpoint("runshutdown");
    auto listener = listen(endpoint);

    std::atomic<bool> returned{ false };
    std::thread server([&]
        {
            auto conn = listener->accept();

            btl::RunLoop loop;

            RemoteApp app;
            app.step = [](std::chrono::microseconds) { return true; };
            app.sync = [] {};
            app.liveWindows = [&] { return RemoteWindows{ a }; };

            RemoteDriver driver(loop, *conn, std::move(app), std::nullopt);

            loop.run();
            returned.store(true);
        });

    auto client = connect(endpoint);
    int id = 1;

    sendRequest(*client, id, "app.run");
    EXPECT_EQ("running",
            awaitResponse(*client, id++).at("result").at("state"));

    // Shutdown while the app free-runs: the driver stops the loop, so the
    // server thread's run() returns.
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
    FakeRemoteWindow a(btl::makeUniqueId(), "w0");
    SessionFixture s("unknown", { a });

    auto reply = s.call("frobnicate");
    EXPECT_EQ(-32601, reply.at("error").at("code").get<int>());
}

TEST(session, malformedJsonIsParseError)
{
    FakeRemoteWindow a(btl::makeUniqueId(), "w0");
    SessionFixture s("malformed", { a });

    auto reply = s.raw("not json");
    EXPECT_EQ(-32700, reply.at("error").at("code").get<int>());
    EXPECT_TRUE(reply.at("id").is_null());
}

TEST(session, missingRequiredParamIsInvalidParams)
{
    FakeRemoteWindow a(btl::makeUniqueId(), "w0");
    SessionFixture s("missingparam", { a });

    // window.introspect requires `window`.
    auto reply = s.call("window.introspect");
    EXPECT_EQ(-32602, reply.at("error").at("code").get<int>());
}

TEST(session, notificationGetsNoReply)
{
    auto idA = btl::makeUniqueId();
    FakeRemoteWindow a(idA, "w0");
    SessionFixture s("notify", { a });

    // A notification (no id) is handled but never answered. If it had drawn a
    // reply, the next receive would read it instead of window.list's response.
    s.notify("app.step", { { "count", 1 } });

    auto reply = s.call("window.list");
    EXPECT_EQ(s.nextId - 1, reply.at("id").get<int>());
    EXPECT_TRUE(reply.contains("result"));

    // The notification did run: one app frame advanced.
    EXPECT_EQ(1, s.steps.load());
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

    // The loop the override platform is injected with; it must outlive the app
    // running on appThread, so it lives here in the test scope.
    btl::RunLoop loop;

    std::thread appThread([&]
    {
        App()
            .platform(ase::makeDummyPlatform(loop))
            .setRemoteEndpoint(endpoint)
            .addWindow(
                    window(bq::signal::constant<std::string>("Agent")),
                    counterWidget(count.handle, count.signal, state))
            .run();
    });

    auto client = listener->accept();
    int id = 1;

    // system.describe: the registry lists the core methods.
    auto describe = rpc(*client, id++, "system.describe");
    bool hasStep = false;
    for (auto const& m : describe.at("result").at("methods"))
        if (m.at("name") == "app.step")
            hasStep = true;
    EXPECT_TRUE(hasStep);

    // window.list: exactly one window; capture its id.
    auto list = rpc(*client, id++, "window.list");
    ASSERT_EQ(1u, list.at("result").at("windows").size());
    uint64_t windowId =
        list.at("result").at("windows").at(0).at("id").get<uint64_t>();
    // The dummy backend cannot read pixels back.
    EXPECT_FALSE(
            list.at("result").at("windows").at(0).at("canCapture").get<bool>());
    auto shot = rpc(*client, id++, "window.screenshot",
            { { "window", windowId } });
    EXPECT_EQ(kCaptureUnsupported, shot.at("error").at("code").get<int>());

    // window.introspect: the counter starts at 0.
    auto intro0 = rpc(*client, id++, "window.introspect",
            { { "window", windowId } });
    auto count0 = findCount(intro0.at("result").at("introspection"));
    ASSERT_TRUE(count0.has_value());
    EXPECT_EQ(0.0, *count0);

    // Act by id: queue a click, step one frame, then observe the bump.
    rpc(*client, id++, "window.inject",
            { { "window", windowId }, { "events", clickEvents(400.0f, 300.0f) } });
    auto step = rpc(*client, id++, "app.step", { { "dt_us", 16667 } });
    EXPECT_EQ("paused", step.at("result").at("state"));

    auto intro1 = rpc(*client, id++, "window.introspect",
            { { "window", windowId } });
    auto count1 = findCount(intro1.at("result").at("introspection"));
    ASSERT_TRUE(count1.has_value());
    EXPECT_EQ(1.0, *count1);

    // window.renderTree: the drawn frame as an avg snapshot, embedded as a
    // value (never a quoted string) with a resolved root in window space.
    auto tree = rpc(*client, id++, "window.renderTree",
            { { "window", windowId } });
    auto const& snap = tree.at("result").at("renderTree");
    ASSERT_TRUE(snap.is_object());
    // The render-tree schema version, owned by bqui's remote serialiser.
    EXPECT_EQ(1, snap.at("version").get<int>());
    EXPECT_FALSE(snap.at("root").is_null());
    EXPECT_GT(snap.at("obb").at("size").at("w").get<double>(), 0.0);
    EXPECT_GT(snap.at("obb").at("size").at("h").get<double>(), 0.0);

    rpc(*client, id++, "app.shutdown");
    appThread.join();
}

// --- Dynamic-windows integration: the window set is live, keyed by id -----

TEST(session, dynamicWindowsOpenAndCloseById)
{
    auto endpoint = uniqueEndpoint("agentdynamic");
    auto listener = listen(endpoint);

    // The loop the override platform is injected with; it must outlive the app
    // running on appThread, so it lives here in the test scope, before `app`.
    btl::RunLoop loop;

    App app;
    app.platform(ase::makeDummyPlatform(loop))
        .setRemoteEndpoint(endpoint);

    // The child closes itself on a click. Its id is known only once it exists,
    // so the handler reads it through a slot filled after construction.
    auto childId = std::make_shared<std::optional<btl::UniqueId>>();
    App childApp = app;
    Window child = window(bq::signal::constant<std::string>("child"));
    auto childWidget =
            shape::rectangle().fill(avg::Color(0.2f, 0.2f, 0.2f, 1.0f))
                | modifier::onClick(1,
                        [childApp, childId](ClickEvent const&) mutable
                        {
                            if (*childId)
                                childApp.removeWindow(**childId);
                        })
                | modifier::setName("childRoot");
    *childId = child.getId();

    // The main window opens the child on a click, once.
    App mainApp = app;
    auto opened = std::make_shared<bool>(false);
    Window main = window(bq::signal::constant<std::string>("main"));
    auto mainWidget =
            shape::rectangle().fill(avg::Color(0.0f, 0.0f, 0.0f, 1.0f))
                | modifier::onClick(1,
                        [mainApp, child, childWidget, opened](
                            ClickEvent const&) mutable
                        {
                            if (!*opened)
                            {
                                *opened = true;
                                mainApp.addWindow(child, childWidget);
                            }
                        })
                | modifier::setName("mainRoot");

    app.addWindow(main, mainWidget);

    std::thread appThread([&] { app.run(); });

    auto client = listener->accept();
    int id = 1;

    // Observe: exactly one window, "main".
    auto list0 = rpc(*client, id++, "window.list");
    ASSERT_EQ(1u, list0.at("result").at("windows").size());
    uint64_t mainId =
        list0.at("result").at("windows").at(0).at("id").get<uint64_t>();

    auto mainTree = rpc(*client, id++, "window.introspect",
            { { "window", mainId } });
    EXPECT_TRUE(treeHasName(mainTree.at("result").at("introspection"),
                "mainRoot"));

    // Act: click main's centre, then step. The handler opens the child, and the
    // next list shows both windows.
    rpc(*client, id++, "window.inject",
            { { "window", mainId }, { "events", clickEvents(400.0f, 300.0f) } });
    rpc(*client, id++, "app.step", { { "dt_us", 16667 } });

    auto list1 = rpc(*client, id++, "window.list");
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

    auto childTree = rpc(*client, id++, "window.introspect",
            { { "window", childWireId } });
    EXPECT_TRUE(treeHasName(childTree.at("result").at("introspection"),
                "childRoot"));

    // Act: click the child's centre, then step. Its handler closes it, and the
    // set is back to just "main".
    rpc(*client, id++, "window.inject",
            { { "window", childWireId },
              { "events", clickEvents(400.0f, 300.0f) } });
    rpc(*client, id++, "app.step", { { "dt_us", 16667 } });

    auto list2 = rpc(*client, id++, "window.list");
    ASSERT_EQ(1u, list2.at("result").at("windows").size());
    EXPECT_EQ(mainId,
            list2.at("result").at("windows").at(0).at("id").get<uint64_t>());

    rpc(*client, id++, "app.shutdown");
    appThread.join();
}

// --- Outgoing queue: a slow reader does not stall the driver ---------------

namespace
{
    void checkStalledReplyDoesNotBlockLaterCommands(SessionFixture& s,
            btl::UniqueId id, std::string const& name)
    {
        int introspectId = s.nextId++;
        sendRequest(*s.client, introspectId, "window.introspect",
                { { "window", id.getValue() } });

        int stepId = s.nextId++;
        sendRequest(*s.client, stepId, "app.step", { { "count", 3 } });

        // The steps run although the reply before them is still unread.
        ASSERT_TRUE(s.awaitSteps(3));

        auto introspection = awaitResponse(*s.client, introspectId);
        EXPECT_TRUE(name == introspection.at("result").at("introspection")
                .at("name").get<std::string>());
        EXPECT_EQ(3u, awaitResponse(*s.client, stepId).at("result")
                .at("frame").get<uint64_t>());
    }

    // A name larger than the OS buffers of the channel, so its reply cannot
    // leave the driver until the client reads.
    std::string const& largeName()
    {
        static std::string const name(8 * 1024 * 1024, 'n');
        return name;
    }

    std::string request(int id, std::string const& method,
            json params = json::object())
    {
        json frame = { { "jsonrpc", "2.0" }, { "id", id },
            { "method", method } };
        if (!params.empty())
            frame["params"] = std::move(params);
        return testkit::framed(frame.dump());
    }

    // Run a driver over `transport` on this thread until it stops its loop.
    // Returns false if it had not stopped after a generous bound.
    bool runDriver(testkit::ScriptedTransport& transport,
            RemoteWindows windows = {}, BlobLimits blobLimits = {},
            std::function<bool(std::chrono::microseconds)> step = {})
    {
        btl::RunLoop loop;
        bool timedOut = false;

        RemoteApp app;
        app.step = step ? std::move(step)
            : [](std::chrono::microseconds) { return true; };
        app.sync = [] {};
        app.liveWindows = [windows] { return windows; };
        RemoteDriver driver(loop, transport, std::move(app), std::nullopt,
                kStepTimeout, blobLimits);

        btl::RunLoop::Timer bound;
        loop.post([&](btl::RunLoop::Controller& controller)
            {
                bound = controller.addTimer(std::chrono::seconds(30),
                        [&](btl::RunLoop::Controller& c)
                        {
                            timedOut = true;
                            c.stop();
                        });
            });

        loop.run();
        return !timedOut;
    }

    std::vector<int> replyIds(std::string const& wire)
    {
        std::vector<int> ids;
        for (auto const& frame : testkit::unframed(wire))
            ids.push_back(json::parse(frame).at("id").get<int>());
        return ids;
    }
} // namespace

TEST(session, stalledReplyDoesNotBlockLaterCommands)
{
    auto idA = btl::makeUniqueId();
    FakeRemoteWindow a(idA, largeName());
    SessionFixture s("stalledreply", { a });

    checkStalledReplyDoesNotBlockLaterCommands(s, idA, largeName());
}

TEST(session, stalledReplyDoesNotBlockLaterCommandsOverTcp)
{
    auto idA = btl::makeUniqueId();
    FakeRemoteWindow a(idA, largeName());
    SessionFixture s(SessionFixture::Tcp{}, { a });

    checkStalledReplyDoesNotBlockLaterCommands(s, idA, largeName());
}

TEST(session, halfClosedTcpClientStillGetsTheLastReply)
{
    auto idA = btl::makeUniqueId();
    FakeRemoteWindow a(idA, largeName());
    SessionFixture s(SessionFixture::Tcp{}, { a });

    int introspectId = s.nextId++;
    sendRequest(*s.client, introspectId, "window.introspect",
            { { "window", idA.getValue() } });

    testkit::shutdownSending(*s.client);

    auto introspection = awaitResponse(*s.client, introspectId);
    ASSERT_TRUE(introspection.is_object());
    EXPECT_TRUE(largeName() == introspection.at("result")
            .at("introspection").at("name").get<std::string>());
    EXPECT_FALSE(s.client->receive());
}

TEST(session, shutdownFlushesAQueuedReplyFirst)
{
    testkit::ScriptedTransport transport;
    transport.incoming = request(1, "window.list")
        + request(2, "app.shutdown");
    transport.writes = { 5, 0, 0, 7, 0 };

    ASSERT_TRUE(runDriver(transport));
    EXPECT_EQ((std::vector<int>{ 1, 2 }), replyIds(transport.wire));
}

TEST(session, halfClosedClientStillGetsQueuedReplies)
{
    testkit::ScriptedTransport transport;
    transport.incoming = request(1, "window.list");
    transport.incomingEnds = true;
    transport.writes = { 5, 0, 0, 7, 0 };

    ASSERT_TRUE(runDriver(transport));
    EXPECT_EQ((std::vector<int>{ 1 }), replyIds(transport.wire));
}

TEST(session, passingTheQueueCapEndsTheSession)
{
    testkit::ScriptedTransport transport(100, false);
    transport.incoming = request(1, "window.list")
        + request(2, "window.list") + request(3, "window.list");
    transport.afterScript = 0;

    ASSERT_TRUE(runDriver(transport));
    EXPECT_TRUE(transport.wire.empty());
}

TEST(session, aWriteErrorEndsTheSession)
{
    testkit::ScriptedTransport transport;
    transport.incoming = request(1, "window.list")
        + request(2, "window.list");
    transport.writes = { 3, std::nullopt };

    ASSERT_TRUE(runDriver(transport));
    EXPECT_EQ(3u, transport.wire.size());
}

TEST(session, shutdownGivesUpOnAClientThatStopsReading)
{
    testkit::ScriptedTransport transport(kMaxQueuedBytes, false);
    transport.incoming = request(1, "window.list")
        + request(2, "app.shutdown");
    transport.afterScript = 0;

    auto started = std::chrono::steady_clock::now();
    ASSERT_TRUE(runDriver(transport));
    EXPECT_GE(std::chrono::steady_clock::now() - started,
            std::chrono::milliseconds(1500));
    EXPECT_TRUE(transport.wire.empty());
}

// --- Blobs: bulk results pulled in chunks ----------------------------------

namespace
{
    struct BlobSession
    {
        explicit BlobSession(BlobLimits limits = {}) :
            limits(limits)
        {
        }

        int send(std::string const& method, json params = json::object())
        {
            int id = nextId++;
            incoming += request(id, method, std::move(params));
            return id;
        }

        int renderTree(bool asBlob)
        {
            json params = { { "window", window.id().getValue() } };
            if (asBlob)
                params["asBlob"] = true;
            return send("window.renderTree", std::move(params));
        }

        int read(json id, json offset, json length = nullptr)
        {
            json params = { { "id", std::move(id) },
                { "offset", std::move(offset) } };
            if (!length.is_null())
                params["length"] = std::move(length);
            return send("blob.read", std::move(params));
        }

        int release(json id)
        {
            return send("blob.release", { { "id", std::move(id) } });
        }

        bool run()
        {
            testkit::ScriptedTransport transport;
            transport.incoming = incoming;
            transport.incomingEnds = true;
            if (!runDriver(transport, { window }, limits))
                return false;

            for (auto const& frame : testkit::unframed(transport.wire))
            {
                json reply = json::parse(frame);
                replies[reply.at("id").get<int>()] = reply;
            }
            return true;
        }

        json const& result(int id) const
        {
            return replies.at(id).at("result");
        }

        int errorCode(int id) const
        {
            return replies.at(id).at("error").at("code").get<int>();
        }

        FakeRemoteWindow window{ btl::makeUniqueId(), "blob" };
        BlobLimits limits;
        std::string incoming;
        int nextId = 1;
        std::map<int, json> replies;
    };

    std::string renderTreeBytes(RemoteWindow const& window)
    {
        json reply = { { "renderTree", toJson(window.snapshot()) } };
        return reply.dump();
    }

    std::string encoded(std::string const& bytes, size_t offset,
            size_t length)
    {
        return base64Encode(bytes.data() + offset, length);
    }
} // namespace

TEST(session, blobReadsWholeAndInRanges)
{
    BlobSession s;
    std::string bytes = renderTreeBytes(s.window);
    ASSERT_GT(bytes.size(), 20u);

    int produced = s.renderTree(true);
    int whole = s.read(1, 0);
    int head = s.read(1, 0, 10);
    int overlap = s.read(1, 5, 10);
    int repeat = s.read(1, 5, 10);
    int tail = s.read(1, bytes.size() - 4, 100);
    int atEnd = s.read(1, bytes.size());
    ASSERT_TRUE(s.run());

    auto const& blob = s.result(produced).at("blob");
    EXPECT_EQ(1u, blob.at("id").get<uint64_t>());
    EXPECT_EQ(bytes.size(), blob.at("size").get<size_t>());
    EXPECT_EQ("application/json", blob.at("contentType"));

    EXPECT_EQ(encoded(bytes, 0, bytes.size()), s.result(whole).at("data"));
    EXPECT_EQ("base64", s.result(whole).at("encoding"));
    EXPECT_TRUE(s.result(whole).at("end").get<bool>());

    EXPECT_EQ(encoded(bytes, 0, 10), s.result(head).at("data"));
    EXPECT_FALSE(s.result(head).at("end").get<bool>());
    EXPECT_EQ(encoded(bytes, 5, 10), s.result(overlap).at("data"));
    EXPECT_EQ(s.result(overlap), s.result(repeat));

    EXPECT_EQ(encoded(bytes, bytes.size() - 4, 4), s.result(tail).at("data"));
    EXPECT_TRUE(s.result(tail).at("end").get<bool>());

    EXPECT_EQ("", s.result(atEnd).at("data"));
    EXPECT_TRUE(s.result(atEnd).at("end").get<bool>());
}

TEST(session, blobReadClampsItsLengthAndDefaultsIt)
{
    BlobLimits limits;
    limits.maxRead = 8;
    limits.defaultRead = 5;
    BlobSession s(limits);
    std::string bytes = renderTreeBytes(s.window);

    s.renderTree(true);
    int clamped = s.read(1, 0, 100);
    int defaulted = s.read(1, 3);
    ASSERT_TRUE(s.run());

    EXPECT_EQ(encoded(bytes, 0, 8), s.result(clamped).at("data"));
    EXPECT_FALSE(s.result(clamped).at("end").get<bool>());
    EXPECT_EQ(encoded(bytes, 3, 5), s.result(defaulted).at("data"));
}

TEST(session, blobReadRejectsABadOffsetOrLength)
{
    BlobSession s;
    std::string bytes = renderTreeBytes(s.window);

    s.renderTree(true);
    std::vector<int> bad = {
        s.read(1, bytes.size() + 1),
        s.read(1, -1),
        s.read(1, 1.5),
        s.read(1, "3"),
        s.send("blob.read", { { "id", 1 } }),
        s.read(1, 0, -1),
        s.read(1, 0, 2.5),
        s.read(1, 0, 0),
    };
    ASSERT_TRUE(s.run());

    for (int id : bad)
        EXPECT_EQ(kInvalidParams, s.errorCode(id)) << "request " << id;
}

TEST(session, theDriverRejectsUnusableBlobLimits)
{
    BlobLimits zeroRead;
    zeroRead.maxRead = 0;
    BlobLimits zeroDefault;
    zeroDefault.defaultRead = 0;
    BlobLimits defaultPastMax;
    defaultPastMax.maxRead = 10;
    defaultPastMax.defaultRead = 11;
    BlobLimits replyPastQueue;
    replyPastQueue.maxRead = kMaxQueuedBytes;

    for (auto const& limits :
            { zeroRead, zeroDefault, defaultPastMax, replyPastQueue })
    {
        testkit::ScriptedTransport transport;
        transport.incomingEnds = true;
        EXPECT_THROW(runDriver(transport, {}, limits), std::invalid_argument);
    }
}

TEST(session, unknownAndReleasedBlobsAreUnknown)
{
    BlobSession s;

    s.renderTree(true);
    int never = s.read(99, 0);
    int released = s.release(1);
    int readReleased = s.read(1, 0);
    int releasedTwice = s.release(1);
    ASSERT_TRUE(s.run());

    EXPECT_EQ(kUnknownBlob, s.errorCode(never));
    EXPECT_TRUE(s.result(released).empty());
    EXPECT_EQ(kUnknownBlob, s.errorCode(readReleased));
    EXPECT_EQ(kUnknownBlob, s.errorCode(releasedTwice));
}

TEST(session, theOutstandingCapFailsTheProducer)
{
    FakeRemoteWindow probe(btl::makeUniqueId(), "blob");
    size_t size = renderTreeBytes(probe).size();

    BlobLimits limits;
    limits.maxOutstanding = 2 * size - 1;
    BlobSession s(limits);

    int first = s.renderTree(true);
    int refused = s.renderTree(true);
    s.release(1);
    int afterRelease = s.renderTree(true);
    ASSERT_TRUE(s.run());

    EXPECT_EQ(1u, s.result(first).at("blob").at("id").get<uint64_t>());

    EXPECT_EQ(kBlobCapReached, s.errorCode(refused));
    auto const& data = s.replies.at(refused).at("error").at("data");
    EXPECT_EQ(size, data.at("inUse").get<size_t>());
    EXPECT_EQ(limits.maxOutstanding, data.at("cap").get<size_t>());

    EXPECT_EQ(2u, s.result(afterRelease).at("blob").at("id").get<uint64_t>());
}

TEST(session, requestsInterleaveBetweenBlobReads)
{
    BlobSession s;
    std::string bytes = renderTreeBytes(s.window);

    s.renderTree(true);
    int head = s.read(1, 0, 12);
    int list = s.send("window.list");
    int step = s.send("app.step");
    int inlineTree = s.renderTree(false);
    int rest = s.read(1, 12, bytes.size());
    int released = s.release(1);
    ASSERT_TRUE(s.run());

    EXPECT_EQ(encoded(bytes, 0, 12), s.result(head).at("data"));
    EXPECT_EQ(1u, s.result(list).at("windows").size());
    EXPECT_EQ(1u, s.result(step).at("frame").get<uint64_t>());
    EXPECT_TRUE(s.result(inlineTree).contains("renderTree"));
    EXPECT_EQ(encoded(bytes, 12, bytes.size() - 12),
            s.result(rest).at("data"));
    EXPECT_TRUE(s.result(rest).at("end").get<bool>());
    EXPECT_TRUE(s.result(released).empty());
}

TEST(session, aRenderTreeBlobHoldsTheInlineReply)
{
    BlobSession s;

    int inlineTree = s.renderTree(false);
    int produced = s.renderTree(true);
    int whole = s.read(1, 0);
    ASSERT_TRUE(s.run());

    std::string bytes = s.result(inlineTree).dump();
    EXPECT_EQ(bytes.size(),
            s.result(produced).at("blob").at("size").get<size_t>());
    EXPECT_EQ(base64Encode(bytes.data(), bytes.size()),
            s.result(whole).at("data"));
}

TEST(session, aNonBooleanAsBlobIsInvalid)
{
    BlobSession s;
    json window = s.window.id().getValue();

    int number = s.send("window.renderTree",
            { { "window", window }, { "asBlob", 1 } });
    int text = s.send("window.renderTree",
            { { "window", window }, { "asBlob", "true" } });
    ASSERT_TRUE(s.run());

    EXPECT_EQ(kInvalidParams, s.errorCode(number));
    EXPECT_EQ(kInvalidParams, s.errorCode(text));
}

TEST(session, describeMarksBlobMethodsAndReportsTheLimits)
{
    BlobLimits limits;
    limits.maxOutstanding = 1000;
    limits.maxRead = 100;
    limits.defaultRead = 10;
    BlobSession s(limits);

    int describe = s.send("system.describe");
    ASSERT_TRUE(s.run());

    std::map<std::string, bool> returnsBlob;
    for (auto const& method : s.result(describe).at("methods"))
    {
        returnsBlob[method.at("name").get<std::string>()] =
            method.at("returnsBlob").get<bool>();
    }

    EXPECT_TRUE(returnsBlob.at("window.renderTree"));
    EXPECT_FALSE(returnsBlob.at("window.introspect"));
    EXPECT_FALSE(returnsBlob.at("blob.read"));
    EXPECT_FALSE(returnsBlob.at("blob.release"));

    auto const& blob = s.result(describe).at("limits").at("blob");
    EXPECT_EQ(1000u, blob.at("maxOutstanding").get<size_t>());
    EXPECT_EQ(100u, blob.at("maxRead").get<size_t>());
    EXPECT_EQ(10u, blob.at("defaultRead").get<size_t>());
    EXPECT_EQ(json::array({ "base64" }), blob.at("encodings"));
}

// --- Screenshots ------------------------------------------------------------

namespace
{
    // Red is the column times 40, green the row from the top times 60, blue
    // 200 and alpha 128.
    ase::FrameCapture syntheticFrame(int width, int height,
            uint64_t presentIndex)
    {
        ase::FrameCapture capture;
        capture.presentIndex = presentIndex;
        capture.image.size = ase::Vector2i(width, height);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                capture.image.pixels.insert(capture.image.pixels.end(), {
                        static_cast<std::uint8_t>(x * 40),
                        static_cast<std::uint8_t>(y * 60), 200, 128 });
            }
        }
        return capture;
    }

    // The PNG of the frame's pixels from `left`, `top` (rows from the top),
    // `width` by `height`, alpha dropped.
    std::string expectedPng(ase::FrameCapture const& capture, int left,
            int top, int width, int height)
    {
        std::vector<std::uint8_t> rgb;
        for (int y = top; y < top + height; ++y)
        {
            for (int x = left; x < left + width; ++x)
            {
                auto const* pixel = capture.image.pixels.data()
                    + (y * capture.image.size.x() + x) * 4;
                rgb.insert(rgb.end(), pixel, pixel + 3);
            }
        }

        return encodePng(static_cast<uint32_t>(width),
                static_cast<uint32_t>(height), rgb.data());
    }

    json screenshotParams(RemoteWindow const& window,
            json extra = json::object())
    {
        extra["window"] = window.id().getValue();
        return extra;
    }

    int errorCodeOf(json const& reply)
    {
        return reply.at("error").at("code").get<int>();
    }
} // namespace

TEST(session, pausedScreenshotRendersAFrameAndAnswersWithAPng)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "shot");
    SessionFixture s("shot", { window });

    int const id = s.nextId++;
    sendRequest(*s.client, id, "window.screenshot", screenshotParams(window));

    auto armed = window.awaitArmed(1);
    ASSERT_EQ(1u, armed.size());
    // Paused, so the driver rendered a frame for the capture, after marking
    // the window due.
    EXPECT_TRUE(s.awaitSteps(1));
    EXPECT_EQ(1, window.frameRequests());

    auto frame = syntheticFrame(4, 3, 7);
    armed.front()(frame);

    auto reply = awaitResponse(*s.client, id);
    ASSERT_TRUE(reply.contains("result")) << reply.dump();
    auto const& result = reply.at("result");
    EXPECT_EQ(4, result.at("width").get<int>());
    EXPECT_EQ(3, result.at("height").get<int>());
    EXPECT_EQ(7u, result.at("presentIndex").get<uint64_t>());
    EXPECT_FALSE(result.contains("pixelsPerUnit"));
    EXPECT_EQ((json{ { "x", 0.0 }, { "y", 0.0 }, { "width", 4.0 },
                { "height", 3.0 } }), result.at("region"));

    auto png = expectedPng(frame, 0, 0, 4, 3);
    auto const& blob = result.at("blob");
    EXPECT_EQ("image/png", blob.at("contentType"));
    EXPECT_EQ(png.size(), blob.at("size").get<size_t>());

    auto read = s.call("blob.read", { { "id", blob.at("id") },
            { "offset", 0 } });
    EXPECT_EQ(base64Encode(png.data(), png.size()),
            read.at("result").at("data").get<std::string>());
    EXPECT_TRUE(read.at("result").at("end").get<bool>());

    // The frame rendered for the capture moved neither time nor the index.
    auto pause = s.call("app.pause");
    EXPECT_EQ(0, pause.at("result").at("frame").get<int>());
}

TEST(session, screenshotOfARegionCropsAndScales)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "region");
    window.setScalingFactor(2.0f);
    SessionFixture s("region", { window });

    int const id = s.nextId++;
    sendRequest(*s.client, id, "window.screenshot", screenshotParams(window,
                { { "region", { { "x", 0.5 }, { "y", 0.0 },
                    { "width", 1.0 }, { "height", 1.0 } } } }));

    auto armed = window.awaitArmed(1);
    ASSERT_EQ(1u, armed.size());
    auto frame = syntheticFrame(6, 4, 2);
    armed.front()(frame);

    auto reply = awaitResponse(*s.client, id);
    ASSERT_TRUE(reply.contains("result")) << reply.dump();
    auto const& result = reply.at("result");
    EXPECT_EQ((json{ { "x", 0.5 }, { "y", 0.0 }, { "width", 1.0 },
                { "height", 1.0 } }), result.at("region"));

    // Logical (0.5, 0) to (1.5, 1) at two pixels a unit is columns 1 to 2
    // of the bottom two rows.
    auto png = expectedPng(frame, 1, 2, 2, 2);
    auto read = s.call("blob.read", { { "id", result.at("blob").at("id") },
            { "offset", 0 } });
    EXPECT_EQ(base64Encode(png.data(), png.size()),
            read.at("result").at("data").get<std::string>());

    int const scaled = s.nextId++;
    sendRequest(*s.client, scaled, "window.screenshot",
            screenshotParams(window, { { "scale", 0.5 } }));
    armed = window.awaitArmed(1);
    ASSERT_EQ(1u, armed.size());
    armed.front()(frame);

    auto half = awaitResponse(*s.client, scaled);
    ASSERT_TRUE(half.contains("result")) << half.dump();
    EXPECT_EQ(3, half.at("result").at("width").get<int>());
    EXPECT_EQ(2, half.at("result").at("height").get<int>());
    EXPECT_EQ((json{ { "x", 0.0 }, { "y", 0.0 }, { "width", 3.0 },
                { "height", 2.0 } }), half.at("result").at("region"));
}

TEST(session, widgetScreenshotCropsToTheBoundsOfTheCapturedFrame)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "widget");
    avg::UniqueId const widget;
    window.setWidget(std::make_pair(widget, LogicalRect{ 1, 0, 2, 1 }));
    SessionFixture s("shotwidget", { window });

    int const id = s.nextId++;
    sendRequest(*s.client, id, "window.screenshot", screenshotParams(window,
                { { "widget", widget.getValue() } }));
    auto armed = window.awaitArmed(1);
    ASSERT_EQ(1u, armed.size());

    // Once a later request is answered, the captured frame has rendered; the
    // widget then moves before the pixels arrive.
    EXPECT_TRUE(s.call("window.list").contains("result"));
    window.setWidget(std::make_pair(widget, LogicalRect{ 0, 0, 1, 1 }));

    auto frame = syntheticFrame(4, 3, 1);
    armed.front()(frame);

    auto reply = awaitResponse(*s.client, id);
    ASSERT_TRUE(reply.contains("result")) << reply.dump();
    auto const& result = reply.at("result");
    EXPECT_EQ((json{ { "x", 1.0 }, { "y", 0.0 }, { "width", 2.0 },
                { "height", 1.0 } }), result.at("region"));

    auto png = expectedPng(frame, 1, 2, 2, 1);
    auto read = s.call("blob.read", { { "id", result.at("blob").at("id") },
            { "offset", 0 } });
    EXPECT_EQ(base64Encode(png.data(), png.size()),
            read.at("result").at("data").get<std::string>());
}

TEST(session, widgetMissingFromTheCapturedFrameIsInvalidParams)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "widgetgone");
    avg::UniqueId const widget;
    window.setWidget(std::make_pair(widget, LogicalRect{ 0, 0, 1, 1 }));
    window.setWidgetAtNextPresent(std::nullopt);
    SessionFixture s("shotwidgetgone", { window });

    int const id = s.nextId++;
    sendRequest(*s.client, id, "window.screenshot", screenshotParams(window,
                { { "widget", widget.getValue() } }));
    auto armed = window.awaitArmed(1);
    ASSERT_EQ(1u, armed.size());

    // Back by the time the pixels arrive, but not in the frame they show.
    EXPECT_TRUE(s.call("window.list").contains("result"));
    window.setWidget(std::make_pair(widget, LogicalRect{ 0, 0, 1, 1 }));
    armed.front()(syntheticFrame(4, 3, 1));

    auto reply = awaitResponse(*s.client, id);
    EXPECT_EQ(kInvalidParams, errorCodeOf(reply)) << reply.dump();
}

TEST(session, pauseRendersNoFrameForAScreenshotWhoseFrameHasRendered)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "pauseafter");
    SessionFixture s("shotpauseafter", { window });

    int const id = s.nextId++;
    sendRequest(*s.client, id, "window.screenshot", screenshotParams(window));
    auto armed = window.awaitArmed(1);
    ASSERT_EQ(1u, armed.size());

    EXPECT_TRUE(s.call("app.pause").contains("result"));
    EXPECT_EQ(1, s.steps.load());

    armed.front()(syntheticFrame(2, 2, 1));
    EXPECT_TRUE(awaitResponse(*s.client, id).contains("result"));
}

TEST(session, pauseRendersAFrameForAScreenshotArmedWhileRunning)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "pauserunning");
    window.serveWith(syntheticFrame(2, 2, 1));
    int steps = 0;

    testkit::ScriptedTransport transport;
    transport.incoming = request(1, "app.run")
        + request(2, "window.screenshot", screenshotParams(window))
        + request(3, "app.pause");
    transport.incomingEnds = true;
    ASSERT_TRUE(runDriver(transport, { window }, {},
                [&](std::chrono::microseconds)
                {
                    ++steps;
                    return true;
                }));

    EXPECT_EQ(1, steps);
    EXPECT_EQ((std::vector<int>{ 1, 3, 2 }), replyIds(transport.wire));
}

TEST(session, screenshotRejectsBadParamsAndUnknownWindows)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "params");
    SessionFixture s("shotparams", { window });

    auto unknown = s.call("window.screenshot",
            { { "window", btl::makeUniqueId().getValue() } });
    EXPECT_EQ(kInvalidParams, errorCodeOf(unknown));

    json const region = { { "x", 0 }, { "y", 0 }, { "width", 1 },
        { "height", 1 } };
    for (json extra : {
            json{ { "region", region }, { "widget", 1 } },
            json{ { "region", { { "x", 0 }, { "y", 0 }, { "width", 0 },
                { "height", 1 } } } },
            json{ { "region", "everything" } },
            json{ { "widget", 12345 } },
            json{ { "scale", 0 } },
            json{ { "scale", 1.5 } },
            json{ { "scale", 1e-50 } },
            json{ { "region", { { "x", 1e39 }, { "y", 0 }, { "width", 1 },
                { "height", 1 } } } },
            })
    {
        auto reply = s.call("window.screenshot",
                screenshotParams(window, extra));
        EXPECT_EQ(kInvalidParams, errorCodeOf(reply)) << extra.dump();
    }

    EXPECT_TRUE(window.takeArmed().empty());
}

TEST(session, screenshotOfAWindowThatCannotCaptureIsUnsupported)
{
    FakeRemoteWindow window(btl::makeUniqueId(), "plain");
    SessionFixture s("shotunsupported", { window });

    auto reply = s.call("window.screenshot", screenshotParams(window));

    EXPECT_EQ(kCaptureUnsupported, errorCodeOf(reply));
    EXPECT_EQ("unsupported",
            reply.at("error").at("data").at("reason").get<std::string>());
    EXPECT_EQ(0, s.steps.load());
}

TEST(session, failedCapturesAnswerWithErrors)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "failing");
    SessionFixture s("shotfailing", { window });

    std::vector<std::pair<ase::ReadbackError, int>> const cases{
        { ase::ReadbackError::Unsupported, kCaptureUnsupported },
        { ase::ReadbackError::Cancelled, kInternalError },
        { ase::ReadbackError::Failed, kInternalError },
    };
    std::vector<std::string> const reasons{ "unsupported", "cancelled",
        "failed" };

    for (size_t i = 0; i < cases.size(); ++i)
    {
        int const id = s.nextId++;
        sendRequest(*s.client, id, "window.screenshot",
                screenshotParams(window));
        auto armed = window.awaitArmed(1);
        ASSERT_EQ(1u, armed.size());
        armed.front()(cases[i].first);

        auto reply = awaitResponse(*s.client, id);
        EXPECT_EQ(cases[i].second, errorCodeOf(reply));
        EXPECT_EQ(reasons[i],
                reply.at("error").at("data").at("reason").get<std::string>());
    }
}

TEST(session, tooManyScreenshotsInFlightIsBusy)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "busy");
    SessionFixture s("shotbusy", { window });

    std::vector<int> ids;
    for (size_t i = 0; i < kMaxScreenshotsInFlight; ++i)
    {
        ids.push_back(s.nextId++);
        sendRequest(*s.client, ids.back(), "window.screenshot",
                screenshotParams(window));
    }

    auto busy = s.call("window.screenshot", screenshotParams(window));
    EXPECT_EQ(kCaptureBusy, errorCodeOf(busy));

    auto armed = window.awaitArmed(kMaxScreenshotsInFlight);
    ASSERT_EQ(kMaxScreenshotsInFlight, armed.size());
    for (auto& callback : armed)
        callback(ase::ReadbackError::Cancelled);

    std::vector<int> answered;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        auto raw = s.client->receive();
        ASSERT_TRUE(raw);
        answered.push_back(json::parse(*raw).at("id").get<int>());
    }
    std::sort(answered.begin(), answered.end());
    EXPECT_EQ(ids, answered);
}

TEST(session, screenshotWhoseEncodeFailsIsAnInternalError)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "badframe");
    SessionFixture s("shotbadframe", { window });

    int const id = s.nextId++;
    sendRequest(*s.client, id, "window.screenshot", screenshotParams(window));
    auto armed = window.awaitArmed(1);
    ASSERT_EQ(1u, armed.size());

    // Fewer pixels than the frame's size claims.
    auto frame = syntheticFrame(4, 3, 1);
    frame.image.pixels.resize(4);
    armed.front()(frame);

    auto reply = awaitResponse(*s.client, id);
    EXPECT_EQ(kInternalError, errorCodeOf(reply)) << reply.dump();

    auto list = s.call("window.list");
    EXPECT_TRUE(list.contains("result"));
}

TEST(session, screenshotPastTheBlobCapFails)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "cap");
    BlobLimits limits;
    limits.maxOutstanding = 16;
    limits.maxRead = 8;
    limits.defaultRead = 8;
    SessionFixture s("shotcap", { window }, kStepTimeout, limits);

    int const id = s.nextId++;
    sendRequest(*s.client, id, "window.screenshot", screenshotParams(window));
    auto armed = window.awaitArmed(1);
    ASSERT_EQ(1u, armed.size());
    armed.front()(syntheticFrame(4, 4, 1));

    auto reply = awaitResponse(*s.client, id);
    EXPECT_EQ(kBlobCapReached, errorCodeOf(reply));
}

TEST(session, screenshotFailsWhenNoFrameIsCapturedInTime)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "late");
    SessionFixture s("shotlate", { window }, std::chrono::milliseconds(50),
            {});

    auto reply = s.call("window.screenshot", screenshotParams(window));
    EXPECT_EQ(kInternalError, errorCodeOf(reply));

    // A capture served after the deadline goes nowhere.
    for (auto& callback : window.takeArmed())
        callback(syntheticFrame(2, 2, 1));
    auto list = s.call("window.list");
    EXPECT_TRUE(list.contains("result"));
}

TEST(session, capturesAWindowHasNotServedAreBounded)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "stale");
    SessionFixture s("shotstale", { window }, std::chrono::milliseconds(50),
            {});

    for (size_t i = 0; i < kMaxScreenshotsInFlight; ++i)
    {
        auto late = s.call("window.screenshot", screenshotParams(window));
        EXPECT_EQ(kInternalError, errorCodeOf(late));
    }

    auto busy = s.call("window.screenshot", screenshotParams(window));
    EXPECT_EQ(kCaptureBusy, errorCodeOf(busy));
    EXPECT_EQ(kMaxScreenshotsInFlight,
            busy.at("error").at("data").at("armed").get<size_t>());

    // Once the window serves the stale captures, it can be armed again.
    auto stale = window.takeArmed();
    ASSERT_EQ(kMaxScreenshotsInFlight, stale.size());
    for (auto& callback : stale)
        callback(syntheticFrame(2, 2, 1));

    int code = kCaptureBusy;
    for (int i = 0; i < 100 && code == kCaptureBusy; ++i)
    {
        code = errorCodeOf(s.call("window.screenshot",
                    screenshotParams(window)));
        if (code == kCaptureBusy)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_EQ(kInternalError, code);
}

TEST(session, halfClosedClientStillGetsAScreenshot)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "halfclosed");
    auto frame = syntheticFrame(3, 2, 5);
    window.serveWith(frame);

    testkit::ScriptedTransport transport;
    transport.incoming = request(1, "window.screenshot",
            screenshotParams(window));
    transport.incomingEnds = true;
    ASSERT_TRUE(runDriver(transport, { window }));

    auto frames = testkit::unframed(transport.wire);
    ASSERT_EQ(1u, frames.size());
    auto reply = json::parse(frames.front());
    ASSERT_TRUE(reply.contains("result")) << reply.dump();
    EXPECT_EQ(expectedPng(frame, 0, 0, 3, 2).size(),
            reply.at("result").at("blob").at("size").get<size_t>());
}

TEST(session, shutdownWhileAScreenshotIsPendingAnswersIt)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "ending");

    testkit::ScriptedTransport transport;
    transport.incoming = request(1, "window.screenshot",
            screenshotParams(window)) + request(2, "app.shutdown");
    transport.incomingEnds = true;
    ASSERT_TRUE(runDriver(transport, { window }));

    auto frames = testkit::unframed(transport.wire);
    ASSERT_EQ(2u, frames.size());
    EXPECT_EQ(2, json::parse(frames[0]).at("id").get<int>());
    auto shot = json::parse(frames[1]);
    EXPECT_EQ(1, shot.at("id").get<int>());
    EXPECT_EQ(kInternalError, errorCodeOf(shot));

    // The armed capture outlived the loop it would post to, so it is dropped
    // uncalled.
    EXPECT_EQ(1u, window.takeArmed().size());
}

TEST(session, appQuittingInTheZeroTimeFrameAnswersTheScreenshot)
{
    CapturableFakeWindow window(btl::makeUniqueId(), "quitting");

    testkit::ScriptedTransport transport;
    transport.incoming = request(1, "window.screenshot",
            screenshotParams(window));
    transport.incomingEnds = true;
    ASSERT_TRUE(runDriver(transport, { window }, {},
                [](std::chrono::microseconds) { return false; }));

    auto frames = testkit::unframed(transport.wire);
    ASSERT_EQ(1u, frames.size());
    auto reply = json::parse(frames.front());
    EXPECT_EQ(1, reply.at("id").get<int>());
    EXPECT_EQ(kInternalError, errorCodeOf(reply));
}

// --- Stepping a platform whose window is out of in-flight frames -----------

namespace
{
    // Forwards to the driver's transport, reporting how many requests the
    // driver reads, each frame it queues at the moment it queues it, and when
    // it finds the input ended.
    class RecordingTransport : public Transport
    {
    public:
        RecordingTransport(Transport& inner,
                std::function<void(size_t)> onReceived,
                std::function<void(std::string const&)> onEnqueue,
                std::function<void()> onDisconnected) :
            inner_(inner),
            onReceived_(std::move(onReceived)),
            onEnqueue_(std::move(onEnqueue)),
            onDisconnected_(std::move(onDisconnected))
        {
        }

        void send(std::string const& frame) override { inner_.send(frame); }
        std::optional<std::string> receive() override
        {
            return inner_.receive();
        }
        void close() override { inner_.close(); }
        btl::NativeHandle nativeHandle() override
        {
            return inner_.nativeHandle();
        }
        std::vector<std::string> receiveReady() override
        {
            auto frames = inner_.receiveReady();
            onReceived_(frames.size());
            return frames;
        }
        bool disconnected() const override
        {
            bool ended = inner_.disconnected();
            if (ended)
                onDisconnected_();
            return ended;
        }
        void enqueue(std::string const& frame) override
        {
            onEnqueue_(frame);
            inner_.enqueue(frame);
        }
        bool flushReady() override { return inner_.flushReady(); }
        btl::NativeHandle writableHandle() override
        {
            return inner_.writableHandle();
        }

    private:
        Transport& inner_;
        std::function<void(size_t)> onReceived_;
        std::function<void(std::string const&)> onEnqueue_;
        std::function<void()> onDisconnected_;
    };

    // Serve the driver over a paused dummy platform with one window that is
    // due every frame and whose fences are held, so the window runs out of
    // in-flight frames after two steps. The counters are what the test waits
    // on in place of sleeping.
    struct SteppedPlatformFixture
    {
        struct QueuedReply
        {
            int id;
            int renderedBefore;
        };

        std::unique_ptr<TransportListener> listener;
        std::unique_ptr<Transport> client;
        std::thread serverThread;
        int nextId = 1;

        btl::RunLoop loop;

        // Loop thread only.
        ase::DummyRenderContext* context = nullptr;
        std::optional<RemoteDriver> driver;

        std::mutex mutex;
        std::condition_variable changed;
        int appFrames = 0;
        int rendered = 0;
        int inputEnds = 0;
        size_t received = 0;
        std::vector<QueuedReply> replies;

        explicit SteppedPlatformFixture(std::string const& name,
                std::chrono::milliseconds stepTimeout = kStepTimeout)
        {
            auto endpoint = uniqueEndpoint(name);
            listener = listen(endpoint);
            start(endpoint, stepTimeout);
        }

        struct Tcp {};

        explicit SteppedPlatformFixture(Tcp)
        {
            auto tcp = tcpListen(*parseTcpEndpoint("127.0.0.1:0"));
            auto endpoint = "127.0.0.1:" + std::to_string(tcp->port());
            listener = std::move(tcp);
            start(endpoint, kStepTimeout);
        }

        void start(std::string const& endpoint,
                std::chrono::milliseconds stepTimeout)
        {
            serverThread = std::thread([this, stepTimeout]
                {
                    auto server = listener->accept();

                    ase::Platform platform = ase::makeDummyPlatform(loop);
                    ase::RenderContext renderContext
                        = platform.makeRenderContext();
                    context = &renderContext
                        .getImpl<ase::DummyRenderContext>();

                    ase::Window window = platform.makeWindow(renderContext,
                            ase::Vector2i(64, 64), false);
                    window.setFrameCallback([this](ase::Frame const&)
                        {
                            count(rendered);
                            return std::make_optional(
                                    std::chrono::microseconds(0));
                        });

                    context->holdFences();

                    RecordingTransport transport(*server,
                            [this](size_t frames)
                            {
                                {
                                    std::lock_guard<std::mutex> lock(mutex);
                                    received += frames;
                                }
                                changed.notify_all();
                            },
                            [this](std::string const& frame)
                            {
                                record(frame);
                            },
                            [this] { count(inputEnds); });

                    RemoteApp app;
                    app.step = [](std::chrono::microseconds) { return true; };
                    app.sync = [] {};
                    app.liveWindows = [] { return RemoteWindows(); };

                    driver.emplace(loop, transport, std::move(app),
                            platform.pause(), stepTimeout);

                    platform.run([this](ase::Frame const&)
                        {
                            count(appFrames);
                            return true;
                        });

                    driver.reset();
                    context->releaseFences();
                    context = nullptr;
                });

            client = connect(endpoint);
        }

        ~SteppedPlatformFixture()
        {
            client.reset();
            if (serverThread.joinable())
                serverThread.join();
        }

        json call(std::string const& method, json params = json::object())
        {
            return rpc(*client, nextId++, method, std::move(params));
        }

        int send(std::string const& method, json params = json::object())
        {
            int id = nextId++;
            sendRequest(*client, id, method, std::move(params));
            return id;
        }

        void waitForAppFrames(int frames)
        {
            std::unique_lock<std::mutex> lock(mutex);
            changed.wait(lock, [&] { return appFrames >= frames; });
        }

        void waitForReceived(size_t requests)
        {
            std::unique_lock<std::mutex> lock(mutex);
            changed.wait(lock, [&] { return received >= requests; });
        }

        void waitForInputEnd()
        {
            std::unique_lock<std::mutex> lock(mutex);
            changed.wait(lock, [&] { return inputEnds > 0; });
        }

        void releaseFences()
        {
            loop.post([this](btl::RunLoop::Controller&)
                {
                    context->releaseFences();
                });
        }

        int renderedCount()
        {
            std::lock_guard<std::mutex> lock(mutex);
            return rendered;
        }

        int appFrameCount()
        {
            std::lock_guard<std::mutex> lock(mutex);
            return appFrames;
        }

        std::vector<QueuedReply> queuedReplies()
        {
            std::lock_guard<std::mutex> lock(mutex);
            return replies;
        }

    private:
        void count(int& counter)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++counter;
            }
            changed.notify_all();
        }

        void record(std::string const& frame)
        {
            json message = json::parse(frame, nullptr, false);
            auto id = message.find("id");
            if (id == message.end() || !id->is_number())
                return;

            std::lock_guard<std::mutex> lock(mutex);
            replies.push_back({ id->get<int>(), rendered });
        }
    };
} // namespace

TEST(session, stepRepliesOnlyAfterItsFrameRendered)
{
    SteppedPlatformFixture s("stepwaits");

    EXPECT_EQ(1u, s.call("app.step").at("result").at("frame").get<uint64_t>());
    EXPECT_EQ(2u, s.call("app.step").at("result").at("frame").get<uint64_t>());
    ASSERT_EQ(2, s.renderedCount());

    // The window is out of in-flight frames: the third frame cannot render
    // until a fence completes.
    int stepId = s.send("app.step");
    int pauseId = s.send("app.pause");

    // The driver has read the pause before the window's slot frees.
    s.waitForReceived(4);
    s.releaseFences();

    // Read both replies in whichever order they come.
    std::map<int, json> answers;
    while (answers.size() < 2)
    {
        auto raw = s.client->receive();
        ASSERT_TRUE(raw);
        json reply = json::parse(*raw);
        answers[reply.at("id").get<int>()] = reply;
    }

    EXPECT_EQ(3u, answers[stepId].at("result").at("frame").get<uint64_t>());
    EXPECT_EQ(3u, answers[pauseId].at("result").at("frame").get<uint64_t>());

    // The step answered once its window had rendered, and the request behind
    // it waited for that.
    auto replies = s.queuedReplies();
    ASSERT_EQ(4u, replies.size());
    EXPECT_EQ(stepId, replies[2].id);
    EXPECT_EQ(3, replies[2].renderedBefore);
    EXPECT_EQ(pauseId, replies[3].id);
    EXPECT_EQ(3, replies[3].renderedBefore);
}

TEST(session, multiFrameStepRendersEachFrameBeforeTheNext)
{
    SteppedPlatformFixture s("stepeach");

    s.call("app.step", { { "count", 2 } });
    ASSERT_EQ(2, s.renderedCount());

    int stepId = s.send("app.step", { { "count", 3 } });

    s.waitForAppFrames(3);

    int appFramesAtRelease = 0;
    int renderedAtRelease = 0;
    s.loop.post([&](btl::RunLoop::Controller&)
        {
            appFramesAtRelease = s.appFrameCount();
            renderedAtRelease = s.renderedCount();
            s.context->releaseFences();
        });

    auto step = awaitResponse(*s.client, stepId);
    EXPECT_EQ(5u, step.at("result").at("frame").get<uint64_t>());

    // The step stopped at the frame its window could not render.
    EXPECT_EQ(3, appFramesAtRelease);
    EXPECT_EQ(2, renderedAtRelease);
    EXPECT_EQ(5, s.renderedCount());
}

TEST(session, disconnectWhileAStepIsPendingEndsAtTheDeadline)
{
    SteppedPlatformFixture s("stepdisconnect", std::chrono::milliseconds(100));

    s.call("app.step", { { "count", 2 } });

    s.send("app.step");
    s.waitForAppFrames(3);

    // The fixture releases the fences once the session is over and the driver
    // is gone.
    s.client.reset();
    s.serverThread.join();

    EXPECT_EQ(2, s.renderedCount());
}

TEST(session, halfClosedClientStillGetsADeferredStepReply)
{
    SteppedPlatformFixture s(SteppedPlatformFixture::Tcp{});

    s.call("app.step", { { "count", 2 } });

    int stepId = s.send("app.step");
    int pauseId = s.send("app.pause");
    testkit::shutdownSending(*s.client);

    s.waitForInputEnd();
    s.releaseFences();

    auto step = awaitResponse(*s.client, stepId);
    auto pause = awaitResponse(*s.client, pauseId);
    ASSERT_TRUE(step.is_object());
    ASSERT_TRUE(pause.is_object());
    EXPECT_EQ(3u, step.at("result").at("frame").get<uint64_t>());
    EXPECT_EQ(3u, pause.at("result").at("frame").get<uint64_t>());

    EXPECT_FALSE(s.client->receive());
    s.serverThread.join();
}

TEST(session, stepResumedAfterTheDriverIsGoneDoesNothing)
{
    SteppedPlatformFixture s("stepdriverdead");

    s.call("app.step", { { "count", 2 } });

    int stepId = s.send("app.step");
    s.waitForAppFrames(3);

    // Freeing the slot queues the step's resumption; the driver is destroyed
    // before the loop gets to it.
    s.loop.post([&s](btl::RunLoop::Controller& controller)
        {
            s.context->releaseFences();

            controller.post([&s](btl::RunLoop::Controller& c)
                {
                    s.driver.reset();
                    c.post([](btl::RunLoop::Controller& last)
                        {
                            last.stop();
                        });
                });
        });

    s.serverThread.join();

    for (auto const& reply : s.queuedReplies())
        EXPECT_NE(stepId, reply.id);
}

TEST(session, stepFailsWhenItsFrameDoesNotRenderInTime)
{
    SteppedPlatformFixture s("stepdeadline", std::chrono::milliseconds(100));

    s.call("app.step", { { "count", 2 } });

    int stepId = s.send("app.step");
    int pauseId = s.send("app.pause");

    auto step = awaitResponse(*s.client, stepId);
    auto pause = awaitResponse(*s.client, pauseId);
    EXPECT_EQ(kInternalError, step.at("error").at("code").get<int>());
    EXPECT_EQ(3u, pause.at("result").at("frame").get<uint64_t>());

    // The wake from the freed slot comes after the step gave up, and finds
    // nothing to resume.
    std::mutex mutex;
    std::condition_variable woken;
    bool wakeDone = false;
    s.loop.post([&](btl::RunLoop::Controller& controller)
        {
            s.context->releaseFences();
            controller.post([&](btl::RunLoop::Controller&)
                {
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        wakeDone = true;
                    }
                    woken.notify_all();
                });
        });
    {
        std::unique_lock<std::mutex> lock(mutex);
        woken.wait(lock, [&] { return wakeDone; });
    }

    EXPECT_EQ(3u, s.queuedReplies().size());
    EXPECT_EQ(2, s.renderedCount());

    // The next step first renders the frame the window was still owed.
    EXPECT_EQ(4u, s.call("app.step").at("result").at("frame")
            .get<uint64_t>());
    EXPECT_EQ(4, s.renderedCount());
}

TEST(session, shutdownBehindAHungStepGoesAheadAtTheDeadline)
{
    SteppedPlatformFixture s("stephung", std::chrono::milliseconds(100));

    s.call("app.step", { { "count", 2 } });

    int stepId = s.send("app.step");
    int shutdownId = s.send("app.shutdown");

    auto step = awaitResponse(*s.client, stepId);
    auto shutdown = awaitResponse(*s.client, shutdownId);
    EXPECT_EQ(kInternalError, step.at("error").at("code").get<int>());
    EXPECT_TRUE(shutdown.at("result").is_object());

    s.serverThread.join();
    EXPECT_EQ(2, s.renderedCount());
}
