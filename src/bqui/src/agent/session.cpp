#include "bqui/agent/session.h"

#include "introspectionjson.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <iterator>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace bqui::agent
{

using nlohmann::json;

AgentWindow::~AgentWindow() = default;

namespace
{

// Standard JSON-RPC 2.0 error codes.
constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;
constexpr int kInternalError = -32603;

// One nominal frame at ~60 Hz — the default step when a client gives no dt.
constexpr int64_t kNominalFrameUs = 16667;

// A handler failure carrying a JSON-RPC error code. The dispatcher turns it
// into the message's error response (or drops it, for a notification).
struct RpcError
{
    int code;
    std::string message;
    json data = nullptr;
};

// A method parameter's contract, surfaced verbatim by system.describe so an
// external tool can generate its toolset from the registry alone.
struct ParamSpec
{
    char const* name;
    char const* type;
    bool required;
};

// A registry entry: identity, its one-line doc, its parameter schema, and the
// handler the dispatcher routes to. The registry is the single source of truth.
struct Method
{
    std::string name;
    std::string doc;
    std::vector<ParamSpec> params;
    std::function<json(json const&)> handler;
};

// --- Field readers: type-checked, defaulting rather than throwing ---------

double numberField(json const& obj, char const* key, double fallback)
{
    auto it = obj.find(key);
    if (it != obj.end() && it->is_number())
        return it->get<double>();
    return fallback;
}

std::string stringField(json const& obj, char const* key)
{
    auto it = obj.find(key);
    if (it != obj.end() && it->is_string())
        return it->get<std::string>();
    return std::string();
}

bool boolField(json const& obj, char const* key, bool fallback)
{
    auto it = obj.find(key);
    if (it != obj.end() && it->is_boolean())
        return it->get<bool>();
    return fallback;
}

// --- Injection: same event shapes as the interim protocol -----------------

void applyInjection(AgentWindow& window, json const& event)
{
    if (!event.is_object())
        return;

    auto kind = stringField(event, "kind");
    ase::Vector2f pos(
            static_cast<float>(numberField(event, "x", 0.0)),
            static_cast<float>(numberField(event, "y", 0.0)));
    auto pointer = static_cast<unsigned int>(numberField(event, "pointer", 0.0));

    if (kind == "pointerButton")
    {
        auto state = stringField(event, "state");
        window.injectPointerButton(pointer,
                static_cast<unsigned int>(numberField(event, "button", 1.0)),
                pos,
                state == "up" ? ase::ButtonState::up : ase::ButtonState::down);
    }
    else if (kind == "pointerMove")
    {
        window.injectPointerMove(pointer, pos);
    }
    else if (kind == "hover")
    {
        window.injectHover(pointer, pos, boolField(event, "state", true));
    }
    else if (kind == "key")
    {
        auto state = stringField(event, "state");
        window.injectKey(
                state == "up" ? ase::KeyState::up : ase::KeyState::down,
                static_cast<ase::KeyCode>(
                    static_cast<unsigned int>(numberField(event, "code", 0.0))),
                static_cast<uint32_t>(numberField(event, "mods", 0.0)),
                stringField(event, "text"));
    }
    else if (kind == "text")
    {
        window.injectText(stringField(event, "text"));
    }
}

// The live window addressed by id, or null if none carries it.
AgentWindow* findWindow(AgentWindows const& windows, uint64_t id)
{
    for (auto& window : windows)
        if (window.get().id().getValue() == id)
            return &window.get();

    return nullptr;
}

uint64_t requireWindowId(json const& params)
{
    auto it = params.find("window");
    if (it == params.end() || !it->is_number())
        throw RpcError{ kInvalidParams,
            "'window' must be a numeric window id" };

    return it->get<uint64_t>();
}

/**
 * @brief The one object shared by the reader thread and the app thread.
 *
 * A thread-safe FIFO of inbound frames plus a closed flag. The reader thread
 * only `push`es (and `close`s on end-of-stream); the app thread only consumes
 * (`waitPop` when paused, `drainAll` when running) and observes `closed`. A
 * mutex plus condition variable is the whole synchronisation boundary between
 * the two threads — nothing else is shared.
 */
class CommandQueue
{
public:
    /** @brief Enqueue one frame and wake a waiting consumer (reader thread). */
    void push(std::string frame)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            frames_.push_back(std::move(frame));
        }
        cv_.notify_all();
    }

    /** @brief Mark the channel closed and wake every waiter (reader thread). */
    void close()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

    /**
     * @brief Block for the next frame; nullopt once closed and drained.
     *
     * The paused app thread parks here rather than spinning.
     */
    std::optional<std::string> waitPop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return closed_ || !frames_.empty(); });

        if (frames_.empty())
            return std::nullopt;

        std::string frame = std::move(frames_.front());
        frames_.pop_front();
        return frame;
    }

    /** @brief Take every currently-queued frame without blocking. */
    std::vector<std::string> drainAll()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> out(
                std::make_move_iterator(frames_.begin()),
                std::make_move_iterator(frames_.end()));
        frames_.clear();
        return out;
    }

    /** @brief Whether the reader has signalled end-of-stream. */
    bool closed() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    /**
     * @brief Block until closed, or until `timeout` elapses.
     *
     * The app thread uses this during shutdown: the reader closes the queue on
     * its way out, so a return of true means the reader has finished and can be
     * joined without blocking.
     */
    bool waitClosedFor(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return closed_; });
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::string> frames_;
    bool closed_ = false;
};

/**
 * @brief The inspector-protocol server: a method registry and its dispatch loop.
 *
 * Holds the app seam, the monotonic frame counter, and the run state. Launches
 * paused: run() starts a reader thread feeding a command queue, then runs a
 * single-threaded state machine over {PAUSED, RUNNING} — the app thread is the
 * only one that touches windows, sends responses, and emits notifications.
 */
class Session
{
public:
    explicit Session(AgentApp const& app) : app_(app)
    {
        buildRegistry();
    }

    void run(Transport& transport)
    {
        CommandQueue queue;

        // The reader thread does one thing: decode frames and enqueue them. It
        // never sends and never touches windows, so the queue is the only object
        // it shares with the app thread.
        std::thread reader([&transport, &queue]
            {
                for (;;)
                {
                    auto frame = transport.receive();
                    if (!frame)
                    {
                        queue.close(); // Peer gone: end-of-stream.
                        break;
                    }
                    queue.push(std::move(*frame));
                }
            });

        try
        {
            runStateMachine(transport, queue);
        }
        catch (std::exception const&)
        {
            // A send that fails mid-session (the peer vanished, so a response or
            // frame notification hit a broken channel) is end-of-session, not a
            // fault to propagate: fall through to the clean shutdown below, which
            // wakes and joins the reader exactly as an orderly exit would.
        }

        // Clean shutdown: wake the reader out of its blocked receive() so it can
        // be joined. close() unblocks an in-progress receive() with nullopt and
        // makes every later one return nullopt too. The reader acknowledges by
        // closing the queue as it exits, so re-nudge until it does — covering the
        // narrow window where close() lands just before the reader blocks. The
        // queue mutex is never held across the join.
        transport.close();
        while (!queue.waitClosedFor(std::chrono::milliseconds(5)))
            transport.close();

        reader.join();
    }

private:
    enum class State { Paused, Running };

    // The single-threaded state machine. The app thread owns every window, the
    // send side, and the frame counter; it drains the queue at a frame boundary
    // so a query always sees a whole, untorn frame.
    void runStateMachine(Transport& transport, CommandQueue& queue)
    {
        state_ = State::Paused;
        shuttingDown_ = false;

        while (!shuttingDown_)
        {
            if (state_ == State::Running)
            {
                // Drain and dispatch every pending command at this boundary.
                for (auto& frame : queue.drainAll())
                {
                    dispatch(transport, frame);
                    if (shuttingDown_)
                        break;
                }

                if (shuttingDown_)
                    break;

                // A closed queue with nothing left to do means the peer is gone.
                if (queue.closed())
                    break;

                // Still running: advance exactly one frame, then announce it.
                if (state_ == State::Running)
                {
                    advanceFrame(std::chrono::microseconds(kNominalFrameUs));
                    if (frameNotifications_)
                        sendFrameNotification(transport,
                                std::chrono::microseconds(kNominalFrameUs));
                }
            }
            else
            {
                // Paused: park on the queue until a command arrives or it closes.
                auto frame = queue.waitPop();
                if (!frame)
                    break; // Closed and drained: the peer is gone.

                dispatch(transport, *frame);
            }
        }
    }

    // Decode, route, and answer one frame from this (the app) thread — the only
    // thread that sends. A notification draws no reply.
    void dispatch(Transport& transport, std::string const& frame)
    {
        if (auto reply = handleFrame(frame))
            transport.send(*reply);
    }

    // Advance every live window by dt, then reconcile one fused app frame so any
    // window opened or closed this frame materialises. Re-fetch the set each
    // time, since a frame can change it. Bumps the monotonic frame counter.
    void advanceFrame(std::chrono::microseconds dt)
    {
        auto windows = app_.liveWindows();
        for (auto& window : windows)
            window.get().advance(dt);

        app_.reconcile(dt);
        ++frame_;
    }

    void sendFrameNotification(Transport& transport, std::chrono::microseconds dt)
    {
        json note = {
            { "jsonrpc", "2.0" },
            { "method", "frame" },
            { "params", {
                { "index", frame_ },
                { "dt_us", dt.count() },
            } },
        };
        transport.send(note.dump());
    }

    Method const* findMethod(std::string const& name) const
    {
        for (auto const& method : registry_)
            if (method.name == name)
                return &method;

        return nullptr;
    }

    // Decode one frame, route it, and produce its reply — or nullopt when the
    // message is a notification (no `id`) and so draws no response.
    std::optional<std::string> handleFrame(std::string const& frame)
    {
        json request = json::parse(frame, nullptr, /*allow_exceptions=*/false);

        if (request.is_discarded())
            return errorFrame(json(), kParseError, "Parse error");

        if (!request.is_object())
            return errorFrame(json(), kInvalidRequest, "Invalid request");

        json id = request.contains("id") ? request["id"] : json();

        auto methodIt = request.find("method");
        if (methodIt == request.end() || !methodIt->is_string())
            return errorFrame(id, kInvalidRequest,
                    "Invalid request: 'method' must be a string");

        // A well-formed request without an `id` is a notification: handled,
        // never answered — not even on error.
        bool notification = !request.contains("id");

        std::string method = methodIt->get<std::string>();
        json params = request.contains("params") ? request["params"]
                                                  : json::object();

        Method const* entry = findMethod(method);
        if (!entry)
            return maybeReply(notification,
                    errorFrame(id, kMethodNotFound,
                        "Method not found: " + method));

        try
        {
            checkRequiredParams(*entry, params);
            json result = entry->handler(params);
            return maybeReply(notification, resultFrame(id, result));
        }
        catch (RpcError const& e)
        {
            return maybeReply(notification,
                    errorFrame(id, e.code, e.message, e.data));
        }
        catch (std::exception const& e)
        {
            return maybeReply(notification,
                    errorFrame(id, kInternalError, e.what()));
        }
    }

    static std::optional<std::string> maybeReply(bool notification,
            std::string reply)
    {
        if (notification)
            return std::nullopt;
        return reply;
    }

    static void checkRequiredParams(Method const& method, json const& params)
    {
        for (auto const& param : method.params)
        {
            if (!param.required)
                continue;

            if (!params.is_object() || !params.contains(param.name))
                throw RpcError{ kInvalidParams,
                    std::string("missing required parameter '")
                        + param.name + "'" };
        }
    }

    static std::string resultFrame(json const& id, json const& result)
    {
        json reply = {
            { "jsonrpc", "2.0" },
            { "id", id },
            { "result", result },
        };
        return reply.dump();
    }

    static std::string errorFrame(json const& id, int code,
            std::string const& message, json const& data = json())
    {
        json error = {
            { "code", code },
            { "message", message },
        };
        if (!data.is_null())
            error["data"] = data;

        json reply = {
            { "jsonrpc", "2.0" },
            { "id", id },
            { "error", error },
        };
        return reply.dump();
    }

    // --- Handlers ---------------------------------------------------------

    json describe(json const&) const
    {
        json methods = json::array();
        for (auto const& method : registry_)
        {
            json params = json::array();
            for (auto const& param : method.params)
                params.push_back({
                        { "name", param.name },
                        { "type", param.type },
                        { "required", param.required },
                        });

            methods.push_back({
                    { "name", method.name },
                    { "doc", method.doc },
                    { "params", params },
                    });
        }

        return { { "methods", methods } };
    }

    json run(json const&)
    {
        state_ = State::Running;
        return { { "state", "running" } };
    }

    json pause(json const&)
    {
        state_ = State::Paused;
        return { { "state", "paused" }, { "frame", frame_ } };
    }

    json step(json const& params)
    {
        auto count = static_cast<int64_t>(numberField(params, "count", 1.0));
        if (count < 0)
            count = 0;

        auto dt = std::chrono::microseconds(static_cast<int64_t>(
                numberField(params, "dt_us",
                    static_cast<double>(kNominalFrameUs))));

        for (int64_t i = 0; i < count; ++i)
            advanceFrame(dt);

        // A step is an explicit, bounded advance: it always leaves the app
        // paused, whichever state it was called from.
        state_ = State::Paused;
        return { { "state", "paused" }, { "frame", frame_ } };
    }

    json setFrameNotifications(json const& params)
    {
        frameNotifications_ = boolField(params, "enabled", false);
        return json::object();
    }

    json shutdown(json const&)
    {
        shuttingDown_ = true;
        return json::object();
    }

    json windowList(json const&)
    {
        // Reconcile (without advancing time) so the set reflects any pending
        // open/close, then enumerate identity only.
        app_.reconcile(std::chrono::microseconds(0));

        json windows = json::array();
        for (auto& window : app_.liveWindows())
            windows.push_back({ { "id", window.get().id().getValue() } });

        return { { "windows", windows } };
    }

    json windowIntrospect(json const& params)
    {
        uint64_t id = requireWindowId(params);

        auto windows = app_.liveWindows();
        AgentWindow* window = findWindow(windows, id);
        if (!window)
            throw RpcError{ kInvalidParams,
                "no live window with id " + std::to_string(id) };

        return { { "introspection", toJson(window->introspect()) } };
    }

    json windowInject(json const& params)
    {
        uint64_t id = requireWindowId(params);

        auto windows = app_.liveWindows();
        AgentWindow* window = findWindow(windows, id);
        if (!window)
            throw RpcError{ kInvalidParams,
                "no live window with id " + std::to_string(id) };

        auto events = params.find("events");
        if (events == params.end() || !events->is_array())
            throw RpcError{ kInvalidParams, "'events' must be an array" };

        // Injected onto the window's inject seam now; the next app.step's
        // advance is what processes them.
        for (auto const& event : *events)
            applyInjection(*window, event);

        return json::object();
    }

    void buildRegistry()
    {
        registry_.push_back({ "system.describe",
            "List every method, its doc, and its parameter schema.",
            {},
            [this](json const& p) { return describe(p); } });

        registry_.push_back({ "app.run",
            "Enter running mode: free-run frames until paused or shut down.",
            {},
            [this](json const& p) { return run(p); } });

        registry_.push_back({ "app.pause",
            "Enter paused mode: hold the current frame until told otherwise.",
            {},
            [this](json const& p) { return pause(p); } });

        registry_.push_back({ "app.step",
            "Advance `count` frames by `dt_us` each, then stay paused.",
            {
                { "count", "number", false },
                { "dt_us", "number", false },
            },
            [this](json const& p) { return step(p); } });

        registry_.push_back({ "app.shutdown",
            "End the session and let the app release its windows.",
            {},
            [this](json const& p) { return shutdown(p); } });

        registry_.push_back({ "app.setFrameNotifications",
            "Toggle per-frame `frame` notifications while running (off by "
            "default).",
            { { "enabled", "boolean", true } },
            [this](json const& p) { return setFrameNotifications(p); } });

        registry_.push_back({ "window.list",
            "List the live windows by id.",
            {},
            [this](json const& p) { return windowList(p); } });

        registry_.push_back({ "window.introspect",
            "The window's resolved widget (introspection) tree.",
            { { "window", "number", true } },
            [this](json const& p) { return windowIntrospect(p); } });

        registry_.push_back({ "window.inject",
            "Queue input events onto a window for the next step.",
            {
                { "window", "number", true },
                { "events", "array", true },
            },
            [this](json const& p) { return windowInject(p); } });
    }

    AgentApp const& app_;
    std::vector<Method> registry_;
    uint64_t frame_ = 0;
    State state_ = State::Paused;
    bool shuttingDown_ = false;
    bool frameNotifications_ = false;
};

} // namespace

void runSession(AgentApp const& app, Transport& transport)
{
    Session(app).run(transport);
}

void runSession(AgentApp const& app, std::string const& endpoint)
{
    auto transport = connect(endpoint);
    runSession(app, *transport);
}

} // namespace bqui::agent
