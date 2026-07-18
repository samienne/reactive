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
#include <vector>

namespace bqui::agent
{
    /**
     * @brief One window an agent observes and drives.
     *
     * The session talks to a window only through this: inject events at
     * window-space coordinates (the ones an agent reads back from an obb), read
     * the resolved introspection, and advance the window one frame with an
     * agent-supplied dt. `App` provides the concrete implementation over its
     * windows.
     */
    class BQUI_EXPORT AgentWindow
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

        /** @brief The current resolved (window-space) introspection. */
        virtual widget::Introspection introspect() const = 0;

        /** @brief Advance this window one frame by `dt` (update then draw). */
        virtual void advance(std::chrono::microseconds dt) = 0;
    };

    /**
     * @brief Serve the agent over `transport` until it quits or disconnects.
     *
     * Synchronous request/response over the connected transport: receive one
     * command, apply it to the windows, send back a snapshot of all of them,
     * repeat. Commands are `step` (route each inject to its target window by
     * index, then advance every window one dt), `snapshot` (observe without
     * advancing), and `quit`. A malformed command is answered with an error,
     * not a crash; a clean channel close ends the loop.
     */
    BQUI_EXPORT void runSession(std::vector<AgentWindow*> const& windows,
            Transport& transport);

    /** @brief Connect to `endpoint` and run a session over all `windows`. */
    BQUI_EXPORT void runSession(std::vector<AgentWindow*> const& windows,
            std::string const& endpoint);
} // namespace bqui::agent
