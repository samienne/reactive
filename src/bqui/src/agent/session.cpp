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

// The target window of an inject event, or none if the index is out of range.
AgentWindow* targetWindow(std::vector<AgentWindow*> const& windows,
        JsonValue const& event)
{
    auto index = static_cast<size_t>(
            event.find("window").value_or(JsonValue()).asNumber(0.0));

    return index < windows.size() ? windows[index] : nullptr;
}

std::string snapshotResponse(std::vector<AgentWindow*> const& windows)
{
    std::string out = "{\"type\":\"snapshot\",\"windows\":[";

    for (size_t i = 0; i < windows.size(); ++i)
    {
        if (i > 0)
            out += ',';

        out += "{\"index\":" + std::to_string(i) + ",\"introspection\":"
            + toJson(windows[i]->introspect()) + "}";
    }

    out += "]}";
    return out;
}

std::string errorResponse(std::string const& message)
{
    return "{\"type\":\"error\",\"message\":\"" + message + "\"}";
}

} // namespace

void runSession(std::vector<AgentWindow*> const& windows, Transport& transport)
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
            auto inject = command->find("inject");
            if (inject && inject->isArray())
                for (auto const& event : inject->asArray())
                    if (auto* window = targetWindow(windows, event))
                        applyInjection(*window, event);

            auto dt = std::chrono::microseconds(static_cast<int64_t>(
                    command->find("dt_us").value_or(JsonValue()).asNumber(0.0)));
            for (auto* window : windows)
                window->advance(dt);

            transport.send(snapshotResponse(windows));
        }
        else if (type == "snapshot")
        {
            transport.send(snapshotResponse(windows));
        }
        else
        {
            transport.send(errorResponse("unknown command type"));
        }
    }
}

void runSession(std::vector<AgentWindow*> const& windows,
        std::string const& endpoint)
{
    auto transport = connect(endpoint);
    runSession(windows, *transport);
}

} // namespace bqui::agent
