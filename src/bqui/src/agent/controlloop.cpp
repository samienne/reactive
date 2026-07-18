#include "bqui/agent/controlloop.h"

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

std::string snapshotResponse(AgentWindow const& window)
{
    return "{\"type\":\"snapshot\",\"introspection\":"
        + toJson(window.introspection())
        + "}";
}

std::string errorResponse(std::string const& message)
{
    return "{\"type\":\"error\",\"message\":\"" + message + "\"}";
}

} // namespace

void runAgentLoop(Transport& transport, AgentWindow& window)
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
                    applyInjection(window, event);

            auto dt = std::chrono::microseconds(static_cast<int64_t>(
                    command->find("dt_us").value_or(JsonValue()).asNumber(0.0)));
            window.step(dt);

            transport.send(snapshotResponse(window));
        }
        else if (type == "snapshot")
        {
            transport.send(snapshotResponse(window));
        }
        else
        {
            transport.send(errorResponse("unknown command type"));
        }
    }
}

} // namespace bqui::agent
