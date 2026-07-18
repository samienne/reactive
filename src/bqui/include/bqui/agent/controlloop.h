#pragma once

#include "bqui/agent/transport.h"

#include "bqui/widget/introspection.h"

#include "bqui/bquivisibility.h"

#include <ase/keycode.h>
#include <ase/keyevent.h>
#include <ase/pointerbuttonevent.h>
#include <ase/vector.h>

#include <chrono>
#include <cstdint>
#include <string>

namespace bqui::agent
{
    /**
     * @brief The single window an agent observes and drives.
     *
     * The control loop talks to a window only through this: inject events at
     * window-space coordinates (the ones an agent reads back from an obb),
     * advance exactly one frame with an agent-supplied dt, and read the resolved
     * introspection. `App` provides the concrete implementation over its window.
     */
    class AgentWindow
    {
    public:
        virtual ~AgentWindow();

        virtual void injectPointerButton(unsigned int pointerIndex,
                unsigned int buttonIndex, ase::Vector2f pos,
                ase::ButtonState state) = 0;
        virtual void injectPointerMove(unsigned int pointerIndex,
                ase::Vector2f pos) = 0;
        virtual void injectHover(unsigned int pointerIndex, ase::Vector2f pos,
                bool state) = 0;
        virtual void injectKey(ase::KeyState state, ase::KeyCode code,
                uint32_t modifiers, std::string text) = 0;
        virtual void injectText(std::string text) = 0;

        /** @brief Advance exactly one frame by `dt` (update then render). */
        virtual void step(std::chrono::microseconds dt) = 0;

        /** @brief The current resolved (absolute window-space) introspection. */
        virtual widget::Introspection introspection() const = 0;
    };

    /**
     * @brief Serve the agent over `transport` until it quits or disconnects.
     *
     * Synchronous request/response: receive one command, apply it to `window`,
     * send back the resolved introspection, repeat. Commands are `step` (inject
     * events then advance one frame), `snapshot` (observe without stepping), and
     * `quit`. A malformed command is answered with an error, not a crash; a
     * clean channel close ends the loop.
     */
    BQUI_EXPORT void runAgentLoop(Transport& transport, AgentWindow& window);
} // namespace bqui::agent
