#include "bqui/remote/session.h"
#include "bqui/remote/transport.h"

#include "introspectionjson.h"

#include <btl/runloop.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <csignal>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace bqui::remote
{

using nlohmann::json;

RemoteWindow::~RemoteWindow() = default;

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

void applyInjection(RemoteWindow& window, json const& event)
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

/**
 * @brief Reject an injection event that is malformed or would escape the seam.
 *
 * A missing or unrecognised `kind` is rejected outright: @ref applyInjection
 * dispatches on it and would otherwise silently drop the event (an empty reply
 * that reads as success), so an event whose `kind` names no known shape is a
 * client mistake worth surfacing, not swallowing.
 *
 * The seam then indexes fixed 15-slot arrays: a pointer id must be `<= 15` and
 * a button number `1..15` (a `0` button underflows into an out-of-bounds
 * write). Throws @ref RpcError with @ref kInvalidParams naming the offending
 * field on the first violation, so the inject handler can reject a batch before
 * it touches the seam.
 */
void validateInjection(json const& event)
{
    if (!event.is_object())
        throw RpcError{ kInvalidParams, "each inject event must be an object" };

    auto kind = stringField(event, "kind");

    bool const isPointer = kind == "pointerButton" || kind == "pointerMove"
        || kind == "hover";

    bool const isKnown = isPointer || kind == "key" || kind == "text";
    if (!isKnown)
        throw RpcError{ kInvalidParams,
            "each inject event needs a known 'kind' (one of pointerButton, "
            "pointerMove, hover, key, text), got '" + kind + "'" };

    if (isPointer)
    {
        double pointer = numberField(event, "pointer", 0.0);
        if (pointer < 0.0 || pointer > 15.0)
            throw RpcError{ kInvalidParams,
                "'pointer' must be in [0, 15], got "
                    + std::to_string(pointer) };
    }

    if (kind == "pointerButton")
    {
        double button = numberField(event, "button", 1.0);
        if (button < 1.0 || button > 15.0)
            throw RpcError{ kInvalidParams,
                "'button' must be in [1, 15], got "
                    + std::to_string(button) };
    }
}

// The live window addressed by id, or null if none carries it.
RemoteWindow* findWindow(RemoteWindows const& windows, uint64_t id)
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
 * @brief The inspector-protocol server: a method registry driven by a run loop.
 *
 * Holds the app seam, the monotonic frame counter, and the run state. Launches
 * paused. run() drives one btl::RunLoop on which the transport is a readable
 * source (commands dispatch as they arrive) and a frame timer advances the app
 * while running. Single-threaded: the loop thread is the only one that touches
 * windows, sends, and the frame counter, so there is no queue or reader thread.
 */
class Session
{
public:
    explicit Session(RemoteApp const& app) : app_(app)
    {
        buildRegistry();
    }

    void run(Transport& transport)
    {
        btl::RunLoop loop;
        loop_ = &loop;
        transport_ = &transport;
        state_ = State::Paused;
        shuttingDown_ = false;
        ticking_ = false;

        // Register the transport and prime it on the loop thread. This first
        // posted task runs before the loop's first wait. Inbound commands arrive
        // as the transport becomes readable and dispatch here; a peer disconnect
        // or an app.shutdown ends the loop.
        loop.post([this](btl::RunLoop::Controller& controller)
        {
            ControllerScope scope(controller_, controller);

            controller.addReadable(transport_->nativeHandle(),
                    [this](btl::RunLoop::Controller& c) { onReadable(c); })
                .detach();

            // A completion-based backend (the Win32 named pipe) only signals its
            // readable event once an overlapped read is pending, so this first
            // receiveReady() kicks that read off (a no-op for readiness-based
            // sockets); dispatch anything already buffered too.
            for (auto& frame : transport_->receiveReady())
            {
                dispatch(frame);
                if (shuttingDown_)
                    break;
            }

            if (shuttingDown_ || transport_->disconnected())
                loop_->stop();
        });

        loop.run();

        loop_ = nullptr;
        transport_ = nullptr;
    }

private:
    enum class State { Paused, Running };

    // The loop-thread Controller, valid only during a callback. A callback sets
    // it through ControllerScope; ensureTicking() reads it to arm the timer.
    struct ControllerScope
    {
        btl::RunLoop::Controller*& slot;
        btl::RunLoop::Controller* prev;
        ControllerScope(btl::RunLoop::Controller*& s, btl::RunLoop::Controller& c)
            : slot(s), prev(s) { slot = &c; }
        ~ControllerScope() { slot = prev; }
    };

    // Dispatch every frame the transport has ready, then stop the loop if the
    // peer is gone or a shutdown was requested.
    void onReadable(btl::RunLoop::Controller& controller)
    {
        ControllerScope scope(controller_, controller);

        for (auto& frame : transport_->receiveReady())
        {
            dispatch(frame);
            if (shuttingDown_)
                break;
        }

        if (shuttingDown_ || transport_->disconnected())
            loop_->stop();
    }

    // Decode, route, and answer one frame. A notification draws no reply.
    void dispatch(std::string const& frame)
    {
        if (auto reply = handleFrame(frame))
            safeSend(*reply);
    }

    // A send to a vanished peer is end-of-session, not a fault to propagate.
    void safeSend(std::string const& frame)
    {
        try
        {
            transport_->send(frame);
        }
        catch (std::exception const&)
        {
            shuttingDown_ = true;
            loop_->stop();
        }
    }

    // While running, advance one frame per tick off a timer. Pausing stops the
    // chain, so the loop then sleeps on the transport until the next command.
    void ensureTicking()
    {
        if (ticking_ || shuttingDown_ || state_ != State::Running)
            return;

        ticking_ = true;
        controller_->addTimer(std::chrono::microseconds(kNominalFrameUs),
                [this](btl::RunLoop::Controller& c) { onTick(c); }).detach();
    }

    void onTick(btl::RunLoop::Controller& controller)
    {
        ControllerScope scope(controller_, controller);
        ticking_ = false;

        if (shuttingDown_)
        {
            loop_->stop();
            return;
        }

        if (state_ != State::Running)
            return;

        advanceFrame(std::chrono::microseconds(kNominalFrameUs));
        if (frameNotifications_)
            sendFrameNotification(std::chrono::microseconds(kNominalFrameUs));

        ensureTicking();
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

    void sendFrameNotification(std::chrono::microseconds dt)
    {
        json note = {
            { "jsonrpc", "2.0" },
            { "method", "frame" },
            { "params", {
                { "index", frame_ },
                { "dt_us", dt.count() },
            } },
        };
        safeSend(note.dump());
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
        ensureTicking();
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
        RemoteWindow* window = findWindow(windows, id);
        if (!window)
            throw RpcError{ kInvalidParams,
                "no live window with id " + std::to_string(id) };

        return { { "introspection", toJson(window->introspect()) } };
    }

    json windowInject(json const& params)
    {
        uint64_t id = requireWindowId(params);

        auto windows = app_.liveWindows();
        RemoteWindow* window = findWindow(windows, id);
        if (!window)
            throw RpcError{ kInvalidParams,
                "no live window with id " + std::to_string(id) };

        auto events = params.find("events");
        if (events == params.end() || !events->is_array())
            throw RpcError{ kInvalidParams, "'events' must be an array" };

        // Validate the whole batch before touching the seam so a malformed
        // event rejects atomically — none of the batch is applied.
        for (auto const& event : *events)
            validateInjection(event);

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

    RemoteApp const& app_;
    std::vector<Method> registry_;
    btl::RunLoop* loop_ = nullptr;
    btl::RunLoop::Controller* controller_ = nullptr;
    Transport* transport_ = nullptr;
    uint64_t frame_ = 0;
    State state_ = State::Paused;
    bool shuttingDown_ = false;
    bool frameNotifications_ = false;
    bool ticking_ = false;
};

} // namespace

void runSession(RemoteApp const& app, Transport& transport)
{
#ifdef SIGPIPE
    // Linux has no per-socket SIGPIPE suppression, so ignore it process-wide;
    // otherwise a client disconnecting mid-write kills the app.
    std::signal(SIGPIPE, SIG_IGN);
#endif
    Session(app).run(transport);
}

void runSession(RemoteApp const& app, std::string const& endpoint)
{
    auto transport = connect(endpoint);
    runSession(app, *transport);
}

} // namespace bqui::remote
