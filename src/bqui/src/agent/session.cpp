#include "bqui/agent/session.h"

#include "introspectionjson.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
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
 * @brief The inspector-protocol server: a method registry and its dispatch loop.
 *
 * Holds the app seam, the monotonic frame counter, and the loop flag. Launches
 * paused: run() blocks on the transport and advances nothing until an app.step
 * arrives.
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
        running_ = true;
        while (running_)
        {
            auto message = transport.receive();
            if (!message)
                break; // A clean channel close ends the session.

            if (auto reply = handleFrame(*message))
                transport.send(*reply);
        }
    }

private:
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

    json step(json const& params)
    {
        auto count = static_cast<int64_t>(numberField(params, "count", 1.0));
        if (count < 0)
            count = 0;

        auto dt = std::chrono::microseconds(static_cast<int64_t>(
                numberField(params, "dt_us",
                    static_cast<double>(kNominalFrameUs))));

        // The existing per-frame model: advance every live window by dt (so its
        // handlers run, opening or closing windows), then reconcile one fused
        // app frame to materialise those changes. Re-fetch each frame, since a
        // frame can change the set.
        for (int64_t i = 0; i < count; ++i)
        {
            auto windows = app_.liveWindows();
            for (auto& window : windows)
                window.get().advance(dt);

            app_.reconcile(dt);
            ++frame_;
        }

        return { { "state", "paused" }, { "frame", frame_ } };
    }

    json shutdown(json const&)
    {
        running_ = false;
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

        // app.run / app.pause and the reader-thread concurrency model are a
        // later layer; app.run is registered as a documented seam that reports
        // it is not built in this configuration.
        registry_.push_back({ "app.run",
            "Enter running mode (not implemented in this build).",
            {},
            [this](json const& p) { return runNotImplemented(p); } });

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

    json runNotImplemented(json const&) const
    {
        throw RpcError{ kInternalError,
            "app.run is not implemented in this build" };
    }

    AgentApp const& app_;
    std::vector<Method> registry_;
    uint64_t frame_ = 0;
    bool running_ = true;
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
