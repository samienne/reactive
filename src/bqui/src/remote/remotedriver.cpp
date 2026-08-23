#include "bqui/remote/remotedriver.h"
#include "bqui/remote/transport.h"

#include "introspectionjson.h"

#include <avg/rendertree/snapshot.h>

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

// A handler failure carrying a JSON-RPC error code.
struct RpcError
{
    int code;
    std::string message;
    json data = nullptr;
};

struct ParamSpec
{
    char const* name;
    char const* type;
    bool required;
};

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

/** @brief Reject an injection event that is malformed or would escape the seam.
 *
 * An unknown `kind` would be silently dropped by applyInjection, so reject it.
 * The seam indexes fixed 15-slot arrays: pointer id must be `<= 15` and button
 * `1..15` (a `0` button underflows into an out-of-bounds write). Throws RpcError
 * naming the first offending field.
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

} // namespace

/** @brief The inspector-protocol server behind RemoteDriver.
 *
 * Launches paused. Registers the transport as a readable source on the injected
 * loop and, while running, advances the app off a loop timer. Single-threaded:
 * only the loop thread touches the app, so there is no queue or reader thread.
 * It does not own the loop; the caller runs it.
 */
class RemoteDriver::Impl
{
public:
    Impl(btl::RunLoop& loop, Transport& transport, RemoteApp app,
            std::optional<ase::PauseToken> pauseToken) :
        loop_(loop),
        transport_(&transport),
        app_(std::move(app)),
        pauseToken_(std::move(pauseToken))
    {
#ifdef SIGPIPE
        // Linux has no per-socket SIGPIPE suppression, so ignore it
        // process-wide; otherwise a client disconnecting mid-write kills the
        // app.
        std::signal(SIGPIPE, SIG_IGN);
#endif
        buildRegistry();

        loop_.post([this](btl::RunLoop::Controller& controller)
        {
            ControllerScope scope(controller_, controller);

            controller.addReadable(transport_->nativeHandle(),
                    [this](btl::RunLoop::Controller& c) { onReadable(c); })
                .detach();

            // A completion-based backend (Win32 named pipe) only signals
            // readable once an overlapped read is pending, so this first
            // receiveReady() kicks that read off (a no-op for socket backends).
            for (auto& frame : transport_->receiveReady())
            {
                dispatch(frame);
                if (shuttingDown_)
                    break;
            }

            if (shuttingDown_ || transport_->disconnected())
                loop_.stop();
        });
    }

private:
    enum class State { Paused, Running };

    // The loop-thread Controller, valid only during a callback.
    struct ControllerScope
    {
        btl::RunLoop::Controller*& slot;
        btl::RunLoop::Controller* prev;
        ControllerScope(btl::RunLoop::Controller*& s, btl::RunLoop::Controller& c)
            : slot(s), prev(s) { slot = &c; }
        ~ControllerScope() { slot = prev; }
    };

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
            loop_.stop();
    }

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
            loop_.stop();
        }
    }

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
            loop_.stop();
            return;
        }

        if (state_ != State::Running)
            return;

        advanceFrame(std::chrono::microseconds(kNominalFrameUs));
        if (shuttingDown_)
        {
            loop_.stop();
            return;
        }

        if (frameNotifications_)
            sendFrameNotification(std::chrono::microseconds(kNominalFrameUs));

        ensureTicking();
    }

    void advanceFrame(std::chrono::microseconds dt)
    {
        bool keepRunning = pauseToken_ ? pauseToken_->step(dt) : app_.step(dt);
        ++frame_;
        if (!keepRunning)
            shuttingDown_ = true;
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

        // A request without an `id` is a notification: never answered, even on
        // error.
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

        for (int64_t i = 0; i < count && !shuttingDown_; ++i)
            advanceFrame(dt);

        // A step always leaves the app paused, whichever state it started from.
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
        app_.sync();

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

    json windowRenderTree(json const& params)
    {
        uint64_t id = requireWindowId(params);

        auto windows = app_.liveWindows();
        RemoteWindow* window = findWindow(windows, id);
        if (!window)
            throw RpcError{ kInvalidParams,
                "no live window with id " + std::to_string(id) };

        return { { "renderTree", toJson(window->snapshot()) } };
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

        // Validate the whole batch first so a malformed event rejects
        // atomically -- none of the batch is applied.
        for (auto const& event : *events)
            validateInjection(event);

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

        registry_.push_back({ "window.renderTree",
            "The window's render-tree snapshot.",
            { { "window", "number", true } },
            [this](json const& p) { return windowRenderTree(p); } });

        registry_.push_back({ "window.inject",
            "Queue input events onto a window for the next step.",
            {
                { "window", "number", true },
                { "events", "array", true },
            },
            [this](json const& p) { return windowInject(p); } });
    }

    btl::RunLoop& loop_;
    btl::RunLoop::Controller* controller_ = nullptr;
    Transport* transport_ = nullptr;
    RemoteApp app_;
    std::optional<ase::PauseToken> pauseToken_;
    std::vector<Method> registry_;
    uint64_t frame_ = 0;
    State state_ = State::Paused;
    bool shuttingDown_ = false;
    bool frameNotifications_ = false;
    bool ticking_ = false;
};

RemoteDriver::RemoteDriver(btl::RunLoop& loop, Transport& transport,
        RemoteApp app, std::optional<ase::PauseToken> pauseToken) :
    impl_(std::make_unique<Impl>(loop, transport, std::move(app),
                std::move(pauseToken)))
{
}

RemoteDriver::~RemoteDriver() = default;

} // namespace bqui::remote
