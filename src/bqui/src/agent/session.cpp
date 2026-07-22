#include "bqui/agent/session.h"

#include "bqui/agent/introspectionjson.h"
#include "bqui/agent/json.h"

#include <cstdint>

namespace bqui::agent
{

AgentWindow::~AgentWindow() = default;

namespace
{

ase::Vector2f readPos(JsonValue const& event)
{
    return ase::Vector2f(
            static_cast<float>(event.find("x").value_or(JsonValue()).asNumber()),
            static_cast<float>(event.find("y").value_or(JsonValue()).asNumber())
            );
}

unsigned int readPointer(JsonValue const& event)
{
    return static_cast<unsigned int>(
            event.find("pointer").value_or(JsonValue()).asNumber(0.0));
}

ase::ButtonState readButtonState(JsonValue const& event)
{
    auto state = event.find("state").value_or(JsonValue()).asString();
    return state == "up" ? ase::ButtonState::up : ase::ButtonState::down;
}

void applyInjection(AgentWindow& window, JsonValue const& event)
{
    auto kind = event.find("kind").value_or(JsonValue()).asString();

    if (kind == "pointerButton")
    {
        window.injectPointerButton(
                readPointer(event),
                static_cast<unsigned int>(
                    event.find("button").value_or(JsonValue()).asNumber(1.0)),
                readPos(event),
                readButtonState(event));
    }
    else if (kind == "pointerMove")
    {
        window.injectPointerMove(readPointer(event), readPos(event));
    }
    else if (kind == "hover")
    {
        window.injectHover(readPointer(event), readPos(event),
                event.find("state").value_or(JsonValue()).asBool(true));
    }
    else if (kind == "key")
    {
        auto state = event.find("state").value_or(JsonValue()).asString();
        window.injectKey(
                state == "up" ? ase::KeyState::up : ase::KeyState::down,
                static_cast<ase::KeyCode>(static_cast<unsigned int>(
                    event.find("code").value_or(JsonValue()).asNumber())),
                static_cast<uint32_t>(
                    event.find("mods").value_or(JsonValue()).asNumber()),
                event.find("text").value_or(JsonValue()).asString());
    }
    else if (kind == "text")
    {
        window.injectText(event.find("text").value_or(JsonValue()).asString());
    }
}

// The target window of an inject event, addressed by window id. An absent
// "window" field defaults to the first live window; an id that names no live
// window (e.g. one that has since closed) resolves to null and is skipped.
AgentWindow* targetWindow(AgentWindows const& windows, JsonValue const& event)
{
    auto field = event.find("window");
    if (!field)
        return windows.empty() ? nullptr : &windows.front().get();

    auto id = btl::UniqueId(static_cast<uint64_t>(field->asNumber(0.0)));

    for (auto& window : windows)
        if (window.get().id() == id)
            return &window.get();

    return nullptr;
}

std::string snapshotResponse(AgentWindows const& windows)
{
    std::string out = "{\"type\":\"snapshot\",\"windows\":[";

    for (size_t i = 0; i < windows.size(); ++i)
    {
        if (i > 0)
            out += ',';

        auto& window = windows[i].get();

        out += "{\"id\":" + std::to_string(window.id().getValue())
            + ",\"title\":" + toJsonString(window.title())
            + ",\"introspection\":" + toJson(window.introspect()) + "}";
    }

    out += "]}";
    return out;
}

std::string errorResponse(std::string const& message)
{
    return "{\"type\":\"error\",\"message\":\"" + message + "\"}";
}

} // namespace

void runSession(AgentApp const& app, Transport& transport)
{
    for (;;)
    {
        auto message = transport.receive();
        if (!message)
            break;

        auto command = parseJson(*message);
        if (!command || !command->isObject())
        {
            transport.send(errorResponse("malformed command"));
            continue;
        }

        auto type = command->find("type").value_or(JsonValue()).asString();

        if (type == "quit")
            break;

        if (type == "step")
        {
            // The windows as they stand before the step: injects route to
            // these, and these advance. A window opened by one of them does
            // not exist to route to, and must not be advanced twice.
            auto windows = app.liveWindows();

            auto inject = command->find("inject");
            if (inject && inject->isArray())
                for (auto const& event : inject->asArray())
                    if (auto* window = targetWindow(windows, event))
                        applyInjection(*window, event);

            auto dt = std::chrono::microseconds(static_cast<int64_t>(
                    command->find("dt_us").value_or(JsonValue()).asNumber(0.0)));

            // Advancing runs the click handlers, which may open or close
            // windows; the reconcile after materialises those changes.
            for (auto& window : windows)
                window.get().advance(dt);

            app.reconcile(dt);

            // Re-fetch: a window opened this step is in the new set and a
            // closed one is gone, so the snapshot shows the step's own effect.
            transport.send(snapshotResponse(app.liveWindows()));
        }
        else if (type == "snapshot")
        {
            // Reconcile without advancing, so a window opened on a prior step
            // (but not yet collected) is visible, without moving any clock.
            app.reconcile(std::chrono::microseconds(0));
            transport.send(snapshotResponse(app.liveWindows()));
        }
        else
        {
            transport.send(errorResponse("unknown command type"));
        }
    }
}

void runSession(AgentApp const& app, std::string const& endpoint)
{
    auto transport = connect(endpoint);
    runSession(app, *transport);
}

} // namespace bqui::agent
